#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <mongo/client/dbclient.h>
#include <Poco/Net/MailMessage.h>
#include <Poco/Net/MailRecipient.h>
#include <Poco/Net/SMTPClientSession.h>
#include "io_service_pool.hpp"
#include "safe_counter.hpp"
#include "receptor.hpp"
#include "ligand.hpp"
#include "grid_map_task.hpp"
#include "monte_carlo_task.hpp"
#include "summary.hpp"
#include "random_forest_test.hpp"

using namespace std;
using namespace std::chrono;
using namespace boost::filesystem;
using namespace boost::iostreams;
using namespace boost::gregorian;
using namespace boost::posix_time;
using namespace mongo;
using namespace bson;
using namespace Poco::Net;

inline static string local_time()
{
	return to_simple_string(microsec_clock::local_time()) + " ";
}

int main(int argc, char* argv[])
{
	// Check the required number of comand line arguments.
	if (argc < 5)
	{
		cout << "idock host user pwd jobs_path [jobid]" << endl;
		return 0;
	}

	// Fetch command line arguments.
	const auto host = argv[1];
	const auto user = argv[2];
	const auto pwd = argv[3];
	const path jobs_path = argv[4];
	const bool phase2only = argc > 5;

	DBClientConnection conn;
	{
		// Connect to host and authenticate user.
		cout << local_time() << "Connecting to " << host << " and authenticating " << user << endl;
		string errmsg;
		if ((!conn.connect(host, errmsg)) || (!conn.auth("istar", user, pwd, errmsg)))
		{
			cerr << local_time() << errmsg << endl;
			return 1;
		}
	}

	// Initialize default values of constant arguments.
	cout << local_time() << "Initializing constants and variables" << endl;
	const auto collection = "istar.idock";
	const auto jobid_fields = BSON("_id" << 1 << "scheduled" << 1);
	const auto param_fields = BSON("_id" << 0 << "ligands" << 1 << "mwt_lb" << 1 << "mwt_ub" << 1 << "lgp_lb" << 1 << "lgp_ub" << 1 << "ads_lb" << 1 << "ads_ub" << 1 << "pds_lb" << 1 << "pds_ub" << 1 << "hbd_lb" << 1 << "hbd_ub" << 1 << "hba_lb" << 1 << "hba_ub" << 1 << "psa_lb" << 1 << "psa_ub" << 1 << "chg_lb" << 1 << "chg_ub" << 1 << "nrb_lb" << 1 << "nrb_ub" << 1);
	const auto compl_fields = BSON("_id" << 0 << "completed" << 1);
	const auto compt_fields = BSON("_id" << 0 << "email" << 1 << "submitted" << 1 << "description" << 1);
	const path ligands_path = "16_lig.pdbqt";
	const path headers_path = "16_hdr.bin";
	const size_t seed = system_clock::now().time_since_epoch().count();
	const size_t num_threads = thread::hardware_concurrency();
	const size_t num_mc_tasks = 64;
	const fl grid_granularity = 0.08;
	const fl max_ligands_per_job = 1e+6;
	const auto epoch = boost::gregorian::date(1970, 1, 1);

	// Calculate the slice split points on the fly.
	const size_t total_ligands = 23129083;
	const size_t num_slices = 10;
	const size_t num_ligands_per_slice = total_ligands / num_slices;
	const size_t spare_ligands = total_ligands - num_ligands_per_slice * num_slices;
	std::array<size_t, num_slices + 1> slices;
	for (size_t i = 0, sum = 0; i <= num_slices; ++i)
	{
		slices[i] = sum;
		sum += num_ligands_per_slice + (i < spare_ligands);
	}

	// Initialize variables for job caching.
	OID _id;
	path job_path, receptor_path, box_path;
	double mwt_lb, mwt_ub, lgp_lb, lgp_ub, ads_lb, ads_ub, pds_lb, pds_ub;
	int num_ligands, hbd_lb, hbd_ub, hba_lb, hba_ub, psa_lb, psa_ub, chg_lb, chg_ub, nrb_lb, nrb_ub;
	fl filtering_probability;
	box b;
	receptor rec;
	size_t num_gm_tasks;
	vector<array3d<fl>> grid_maps(XS_TYPE_SIZE);

	// Initialize program options.
	std::array<double, 3> center, size;
	using namespace boost::program_options;
	options_description box_options("input (required)");
	box_options.add_options()
		("center_x", value<double>(&center[0])->required())
		("center_y", value<double>(&center[1])->required())
		("center_z", value<double>(&center[2])->required())
		("size_x", value<double>(&size[0])->required())
		("size_y", value<double>(&size[1])->required())
		("size_z", value<double>(&size[2])->required())
		;

	// Initialize an io service pool and create worker threads for later use.
	cout << local_time() << "Creating an io service pool of " << num_threads << " worker threads" << endl;
	io_service_pool io(num_threads);
	safe_counter<size_t> cnt;

	// Precalculate the scoring function in parallel.
	cout << local_time() << "Precalculating scoring function in parallel" << endl;
	scoring_function sf;
	{
		// Precalculate reciprocal square root values.
		vector<fl> rs(scoring_function::Num_Samples, 0);
		for (size_t i = 0; i < scoring_function::Num_Samples; ++i)
		{
			rs[i] = sqrt(i * scoring_function::Factor_Inverse);
		}
		BOOST_ASSERT(rs.front() == 0);
		BOOST_ASSERT(rs.back() == scoring_function::Cutoff);

		// Populate the scoring function task container.
		cnt.init(XS_TYPE_SIZE * (XS_TYPE_SIZE + 1) >> 1);
		for (size_t t1 =  0; t1 < XS_TYPE_SIZE; ++t1)
		for (size_t t2 = t1; t2 < XS_TYPE_SIZE; ++t2)
		{
			io.post([&,t1,t2]()
			{
				sf.precalculate(t1, t2, rs);
				cnt.increment();
			});
		}
		cnt.wait();
	}

	// Load a random forest from file.
	cout << local_time() << "Loading a random forest from file" << endl;
	forest f;
	f.load("pdbbind-latest-refined-x42.rf");

	// Initialize a MT19937 random number generator.
	cout << local_time() << "Seeding a MT19937 RNG with " << seed << endl;
	mt19937eng rng(seed);
	boost::random::uniform_real_distribution<fl> u01(0, 1);

	// Precalculate alpha values for determining step size in BFGS.
	std::array<fl, num_alphas> alphas;
	alphas[0] = 1;
	for (size_t i = 1; i < num_alphas; ++i)
	{
		alphas[i] = alphas[i - 1] * 0.1;
	}

	// Reserve space for containers.
	cout << local_time() << "Reserving space for containers" << endl;
	vector<size_t> atom_types_to_populate; atom_types_to_populate.reserve(XS_TYPE_SIZE);
	ptr_vector<ptr_vector<result>> result_containers;
	result_containers.resize(num_mc_tasks);
	for (auto& rc : result_containers) rc.reserve(1);
	ptr_vector<result> results(1);

	// Open files for reading.
	boost::filesystem::ifstream headers(headers_path);
	boost::filesystem::ifstream ligands(ligands_path);

	cout << local_time() << "Entering event loop" << endl;
	bool sleeping = false;
	while (true)
	{
		int slice;
		bool reload = false;
		if (phase2only)
		{
			cout << local_time() << "Running in phase 2 only mode" << endl;
			_id.init(argv[5]);
			reload = true;
		}
		else
		{
			// Fetch an incompleted job in a first-come-first-served manner.
			if (!sleeping) cout << local_time() << "Fetching an incompleted job" << endl;
			BSONObj info;
			conn.runCommand("istar", BSON("findandmodify" << "idock" << "query" << BSON("scheduled" << BSON("$lt" << static_cast<unsigned int>(num_slices))) << "sort" << BSON("submitted" << 1) << "update" << BSON("$inc" << BSON("scheduled" << 1)) << "fields" << jobid_fields), info); // conn.findAndModify() is available since MongoDB C++ Driver legacy-1.0.0
			const auto value = info["value"];
			if (value.isNull())
			{
				// No incompleted jobs. Sleep for a while.
				if (!sleeping) cout << local_time() << "Sleeping" << endl;
				sleeping = true;
				this_thread::sleep_for(chrono::seconds(10));
				continue;
			}
			sleeping = false;
			const auto job = value.Obj();
			slice = job["scheduled"].Int();

			// Determine whether the current job id and parameters need to be refreshed.
			if (_id != job["_id"].OID())
			{
				_id = job["_id"].OID();
				reload = true;
			}
		}
		cout << local_time() << "Executing job " << _id << endl;

		if (reload)
		{
			// Load job parameters from MongoDB.
			cout << local_time() << "Reloading job parameters from database" << endl;
			const auto param = conn.query(collection, QUERY("_id" << _id), 1, 0, &param_fields)->next();
			num_ligands = param["ligands"].Int();
			mwt_lb = param["mwt_lb"].Number();
			mwt_ub = param["mwt_ub"].Number();
			lgp_lb = param["lgp_lb"].Number();
			lgp_ub = param["lgp_ub"].Number();
			ads_lb = param["ads_lb"].Number();
			ads_ub = param["ads_ub"].Number();
			pds_lb = param["pds_lb"].Number();
			pds_ub = param["pds_ub"].Number();
			hbd_lb = param["hbd_lb"].Int();
			hbd_ub = param["hbd_ub"].Int();
			hba_lb = param["hba_lb"].Int();
			hba_ub = param["hba_ub"].Int();
			psa_lb = param["psa_lb"].Int();
			psa_ub = param["psa_ub"].Int();
			chg_lb = param["chg_lb"].Int();
			chg_ub = param["chg_ub"].Int();
			nrb_lb = param["nrb_lb"].Int();
			nrb_ub = param["nrb_ub"].Int();

			// Recalculate filtering_probability.
			filtering_probability = max_ligands_per_job / num_ligands;

			// Initialize paths for box and receptor files.
			job_path = jobs_path / _id.str();
			box_path = job_path / "box.conf";
			receptor_path = job_path / "receptor.pdbqt";

			// Parse the box file.
			cout << local_time() << "Reloading the box file" << endl;
			variables_map vm;
			boost::filesystem::ifstream box_ifs(box_path);
			store(parse_config_file(box_ifs, box_options), vm);
			vm.notify();
			b = box(vec3(center[0], center[1], center[2]), vec3(size[0], size[1], size[2]), grid_granularity);

			// Parse the receptor file.
			cout << local_time() << "Reloading the receptor file" << endl;
			rec = receptor(receptor_path, b);

			// Reserve storage for grid map task container.
			num_gm_tasks = b.num_probes[0];

			// Clear grid maps.
			grid_maps.clear();
			grid_maps.resize(XS_TYPE_SIZE);
		}

		if (!phase2only)
		{
			// Perform phase 1.
			cout << local_time() << "Executing slice " << slice << endl;
			const auto slice_key = lexical_cast<string>(slice);
			const auto beg_lig = slices[slice];
			const auto end_lig = slices[slice + 1];
			headers.seekg(sizeof(size_t) * beg_lig);
			boost::filesystem::ofstream slice_csv(job_path / (slice_key + ".csv"));
			slice_csv.setf(ios::fixed, ios::floatfield);
			slice_csv << setprecision(12); // Dump as many digits as possible in order to recover accurate conformations in summaries.
			for (auto idx = beg_lig; idx < end_lig; ++idx)
			{
				// Locate a ligand.
				size_t header;
				headers.read((char*)&header, sizeof(size_t));
				ligands.seekg(header);

				// Check if the ligand satisfies the filtering conditions.
				string property;
				getline(ligands, property); // REMARK     00000007  277.364     2.51        9   -14.93   0   4  39   0   8    
				const auto mwt = right_cast<fl>(property, 21, 28);
				const auto lgp = right_cast<fl>(property, 30, 37);
				const auto ads = right_cast<fl>(property, 39, 46);
				const auto pds = right_cast<fl>(property, 48, 55);
				const auto hbd = right_cast<int>(property, 57, 59);
				const auto hba = right_cast<int>(property, 61, 63);
				const auto psa = right_cast<int>(property, 65, 67);
				const auto chg = right_cast<int>(property, 69, 71);
				const auto nrb = right_cast<int>(property, 73, 75);
				if (!((mwt_lb <= mwt) && (mwt <= mwt_ub) && (lgp_lb <= lgp) && (lgp <= lgp_ub) && (ads_lb <= ads) && (ads <= ads_ub) && (pds_lb <= pds) && (pds <= pds_ub) && (hbd_lb <= hbd) && (hbd <= hbd_ub) && (hba_lb <= hba) && (hba <= hba_ub) && (psa_lb <= psa) && (psa <= psa_ub) && (chg_lb <= chg) && (chg <= chg_ub) && (nrb_lb <= nrb) && (nrb <= nrb_ub))) continue;

				// Filtering out the ligand randomly according to the maximum number of ligands per job.
				if (u01(rng) > filtering_probability) continue;

				// Obtain ligand ID. ZINC IDs are 8-character long.
				const auto lig_id = property.substr(11, 8);

				// Parse the ligand.
				ligand lig(ligands);

				// Create grid maps on the fly if necessary.
				BOOST_ASSERT(atom_types_to_populate.empty());
				const vector<size_t> ligand_atom_types = lig.get_atom_types();
				for (const auto t : ligand_atom_types)
				{
					BOOST_ASSERT(t < XS_TYPE_SIZE);
					array3d<fl>& grid_map = grid_maps[t];
					if (grid_map.initialized()) continue; // The grid map of XScore atom type t has already been populated.
					grid_map.resize(b.num_probes); // An exception may be thrown in case memory is exhausted.
					atom_types_to_populate.push_back(t);  // The grid map of XScore atom type t has not been populated and should be populated now.
				}
				if (atom_types_to_populate.size())
				{
					cnt.init(num_gm_tasks);
					for (size_t x = 0; x < num_gm_tasks; ++x)
					{
						io.post([&,x]()
						{
							grid_map_task(grid_maps, atom_types_to_populate, x, sf, b, rec);
							cnt.increment();
						});
					}
					cnt.wait();
					atom_types_to_populate.clear();
				}

				// Run Monte Carlo tasks in parallel.
				cnt.init(num_mc_tasks);
				for (size_t i = 0; i < num_mc_tasks; ++i)
				{
					BOOST_ASSERT(result_containers[i].empty());
					BOOST_ASSERT(result_containers[i].capacity() == 1);
					const size_t s = rng();
					io.post([&,i,s]()
					{
						monte_carlo_task(result_containers[i], lig, s, alphas, sf, b, grid_maps);
						cnt.increment();
					});
				}
				cnt.wait();

				// Merge results from all the tasks into one single result container.
				BOOST_ASSERT(results.empty());
				BOOST_ASSERT(results.capacity() == 1);
				const fl required_square_error = static_cast<fl>(4 * lig.num_heavy_atoms); // Ligands with RMSD < 2.0 will be clustered into the same cluster.
				for (size_t i = 0; i < num_mc_tasks; ++i)
				{
					ptr_vector<result>& task_results = result_containers[i];
					BOOST_ASSERT(task_results.capacity() == 1);
					for (auto& task_result : task_results)
					{
						add_to_result_container(results, static_cast<result&&>(task_result), required_square_error);
					}
					task_results.clear();
				}

				// No conformation can be found if the search space is too small.
				if (results.size())
				{
					BOOST_ASSERT(results.size() == 1);
					const result& r = results.front();

					// Rescore conformations with random forest.
					vector<float> v(42);
					for (size_t i = 0; i < lig.num_heavy_atoms; ++i)
					{
						const auto& la = lig.heavy_atoms[i];
						if (la.rf == RF_TYPE_SIZE) continue;
						for (const auto& ra : rec.atoms)
						{
							if (ra.rf == RF_TYPE_SIZE) continue;
							const auto dist_sqr = distance_sqr(r.heavy_atoms[i], ra.coordinate);
							if (dist_sqr >= 144) continue; // RF-Score cutoff 12A
							++v[(la.rf << 2) + ra.rf];
							if (dist_sqr >= 64) continue; // Vina score cutoff 8A
							if (la.xs != XS_TYPE_SIZE && ra.xs != XS_TYPE_SIZE)
							{
								sf.score(v.data() + 36, la.xs, ra.xs, dist_sqr);
							}
						}
					}
					v.back() = lig.flexibility_penalty_factor;
					const auto rfscore = f(v);

					// Dump ligand result to the slice csv file.
					slice_csv << idx << ',' << (r.f * lig.flexibility_penalty_factor) << ',' << rfscore;
					const auto& p = r.conf.position;
					const auto& q = r.conf.orientation;
					slice_csv << ',' << p[0] << ',' << p[1] << ',' << p[2] << ',' << q.a << ',' << q.b << ',' << q.c << ',' << q.d;
					for (const auto t : r.conf.torsions)
					{
						slice_csv << ',' << t;
					}
					slice_csv << '\n';

					// Clear the results of the current ligand.
					results.clear();
				}

				// Report progress.
				conn.update(collection, BSON("_id" << _id), BSON("$inc" << BSON(slice_key << 1)));
			}

			cout << local_time() << "Closing slice csv" << endl;
			slice_csv.close();

			// Increment the completed slice counter.
			cout << local_time() << "Incrementing the completed slice counter" << endl;
			BSONObj compl_obj;
			conn.runCommand("istar", BSON("findandmodify" << "idock" << "query" << BSON("_id" << _id) << "update" << BSON("$inc" << BSON("completed" << 1)) << "fields" << compl_fields), compl_obj);
			if (compl_obj["value"].Obj()["completed"].Int() + 1 < num_slices) continue;
		}

		// Combine slice csv files. Phase 2 starts here.
		cout << local_time() << "Combining slice csv files" << endl;
		ptr_vector<summary> summaries(num_ligands);
		for (size_t s = 0; s < num_slices; ++s)
		{
			// Parse slice csv.
			const auto slice_csv_path = job_path / (lexical_cast<string>(s) + ".csv");
			string line;
			for (boost::filesystem::ifstream slice_csv(slice_csv_path); getline(slice_csv, line);)
			{
				vector<string> tokens;
				tokens.reserve(10);
				for (size_t comma0 = 0; true;)
				{
					const size_t comma1 = line.find(',', comma0 + 1);
					if (comma1 == string::npos)
					{
						tokens.push_back(line.substr(comma0));
						break;
					}
					tokens.push_back(line.substr(comma0, comma1 - comma0));
					comma0 = comma1 + 1;
				}
				// Ignore incorrect lines.
				if (tokens.size() < 10) continue;
				conformation conf(tokens.size() - 10);
				conf.position = vec3(lexical_cast<fl>(tokens[3]), lexical_cast<fl>(tokens[4]), lexical_cast<fl>(tokens[5]));
				conf.orientation = qtn4(lexical_cast<fl>(tokens[6]), lexical_cast<fl>(tokens[7]), lexical_cast<fl>(tokens[8]), lexical_cast<fl>(tokens[9]));
				for (size_t i = 0; i < conf.torsions.size(); ++i)
				{
					conf.torsions[i] = lexical_cast<fl>(tokens[10 + i]);
				}
				summaries.push_back(new summary(lexical_cast<size_t>(tokens[0]), lexical_cast<fl>(tokens[1]), lexical_cast<fl>(tokens[2]), conf));
			}
		}

		// Sort summaries.
		cout << local_time() << "Sorting " << summaries.size() << " ligands" << endl;
		summaries.sort();

		// Determine the number of ligands to write to output files.
		const auto num_summaries = min<size_t>(summaries.size(), num_ligands); // Number of ligands to be written to log.csv.gz
		BOOST_ASSERT(num_summaries <= num_ligands);
		if (summaries.size() > num_ligands)
		{
			cerr << local_time() << "[warning] Invalid number of rows in multiple slice csv's: num_ligands = " << num_ligands << ", summaries.size() = " << summaries.size() << endl;
		}
		const auto num_hits = min<size_t>(num_summaries, 1000); // Number of ligands to be written to ligands.pdbqt.gz
		BOOST_ASSERT(num_hits <= num_ligands);

		// Write results for successfully docked ligands.
		cout << local_time() << "Writing output files" << endl;
		{
			boost::filesystem::ofstream log_csv(job_path / "log.csv.gz");
			boost::filesystem::ofstream ligands_pdbqt(job_path / "ligands.pdbqt.gz");
			filtering_ostream log_csv_gz;
			filtering_ostream ligands_pdbqt_gz;
			log_csv_gz.push(gzip_compressor());
			ligands_pdbqt_gz.push(gzip_compressor());
			log_csv_gz.push(log_csv);
			ligands_pdbqt_gz.push(ligands_pdbqt);
			log_csv_gz.setf(ios::fixed, ios::floatfield);
			ligands_pdbqt_gz.setf(ios::fixed, ios::floatfield);
			log_csv_gz << "ZINC ID,idock score (kcal/mol),RF-Score (pKd),Heavy atoms,Molecular weight (g/mol),Partition coefficient xlogP,Apolar desolvation (kcal/mol),Polar desolvation (kcal/mol),Hydrogen bond donors,Hydrogen bond acceptors,Polar surface area tPSA (A^2),Net charge,Rotatable bonds,SMILES,Substance information,Suppliers and annotations\n" << setprecision(3);
			ligands_pdbqt_gz << "REMARK 901 1\n" << setprecision(3);
			for (auto idx = 0; idx < num_summaries;)
			{
				// Locate the ligand.
				const auto& s = summaries[idx];
				headers.seekg(sizeof(size_t) * s.index);
				size_t header;
				headers.read((char*)&header, sizeof(size_t));
				ligands.seekg(header);

				// Parse the REMARK lines.
				vector<string> remarks(7);
				for (auto& line : remarks)
				{
					getline(ligands, line);
				}
				const string& property = remarks[0];
				const auto lig_id = property.substr(11, 8);
				const auto mwt = right_cast<fl>(property, 21, 28);
				const auto lgp = right_cast<fl>(property, 30, 37);
				const auto ads = right_cast<fl>(property, 39, 46);
				const auto pds = right_cast<fl>(property, 48, 55);
				const auto hbd = right_cast<int>(property, 57, 59);
				const auto hba = right_cast<int>(property, 61, 63);
				const auto psa = right_cast<int>(property, 65, 67);
				const auto chg = right_cast<int>(property, 69, 71);
				const auto nrb = right_cast<int>(property, 73, 75);
				const auto hac = right_cast<int>(remarks[4], 12, 14);

				// Write to log.csv.gz.
				log_csv_gz << lig_id << ',' << s.energy << ',' << s.rfscore << ',' << hac << ',' << mwt << ',' << lgp << ',' << ads << ',' << pds << ',' << hbd << ',' << hba << ',' << psa << ',' << chg << ',' << nrb << ',' << remarks[1].substr(11) << ",http://zinc.docking.org/substance/" << lig_id << ',' << remarks[2].substr(11) << '\n';

				// Only write conformations of the top ligands to ligands.pdbqt.gz.
				if (idx >= num_hits) continue;

				// Parse the ligand.
				ligand lig(ligands);

				// Validate the correctness of the current summary.
				if (s.conf.torsions.size() != lig.num_active_torsions)
				{
					cerr << local_time() << "[warning] Inequal numbers of torsions: ligand index = " << s.index << ", ZIND ID = " << lig_id << ", lig.num_active_torsions = " << lig.num_active_torsions << ", s.conf.torsions.size() = " << s.conf.torsions.size() << endl;
					continue;
				}

				// Create grid maps on the fly if necessary.
				BOOST_ASSERT(atom_types_to_populate.empty());
				const vector<size_t> ligand_atom_types = lig.get_atom_types();
				for (const auto t : ligand_atom_types)
				{
					BOOST_ASSERT(t < XS_TYPE_SIZE);
					array3d<fl>& grid_map = grid_maps[t];
					if (grid_map.initialized()) continue; // The grid map of XScore atom type t has already been populated.
					grid_map.resize(b.num_probes); // An exception may be thrown in case memory is exhausted.
					atom_types_to_populate.push_back(t);  // The grid map of XScore atom type t has not been populated and should be populated now.
				}
				if (atom_types_to_populate.size())
				{
					cnt.init(num_gm_tasks);
					for (size_t x = 0; x < num_gm_tasks; ++x)
					{
						io.post([&,x]()
						{
							grid_map_task(grid_maps, atom_types_to_populate, x, sf, b, rec);
							cnt.increment();
						});
					}
					cnt.wait();
					atom_types_to_populate.clear();
				}

				// Apply conformation.
				fl e, f;
				change g(lig.num_active_torsions);
				lig.evaluate(s.conf, sf, b, grid_maps, -99, e, f, g);
				const auto r = lig.compose_result(e, f, s.conf);

				// Write models to file.
				ligands_pdbqt_gz << "MODEL     " << setw(4) << ++idx << '\n';
				lig.write_model(ligands_pdbqt_gz, remarks, s, r, b, grid_maps);
				ligands_pdbqt_gz << "ENDMDL\n";
			}
		}

		// Set done time.
		cout << local_time() << "Setting done time" << endl;
		const auto millis_since_epoch = duration_cast<chrono::milliseconds>(system_clock::now().time_since_epoch()).count();
		conn.update(collection, BSON("_id" << _id), BSON("$set" << BSON("done" << Date_t(millis_since_epoch))));

		// Send a completion notification email.
		const auto compt_cursor = conn.query(collection, QUERY("_id" << _id), 1, 0, &compt_fields);
		const auto compt = compt_cursor->next();
		const auto email = compt["email"].String();
		cout << local_time() << "Sending an email to " << email << endl;
		MailMessage message;
		message.setSender("idock <noreply@cse.cuhk.edu.hk>");
		message.setSubject("Your idock job has completed");
		message.setContent("Description: " + compt["description"].String() + "\nLigands selected to dock: " + lexical_cast<string>(num_ligands) + "\nSubmitted: " + to_simple_string(ptime(epoch, boost::posix_time::milliseconds(compt["submitted"].Date().millis))) + " UTC\nCompleted: " + to_simple_string(ptime(epoch, boost::posix_time::milliseconds(millis_since_epoch))) + " UTC\nLigands successfully docked: " + lexical_cast<string>(num_summaries) + "\nLigands written to output: " + lexical_cast<string>(num_hits) + "\nResult: http://istar.cse.cuhk.edu.hk/idock/iview/?" + _id.str());
		message.addRecipient(MailRecipient(MailRecipient::PRIMARY_RECIPIENT, email));
		SMTPClientSession session("137.189.91.190");
		session.login();
		session.sendMessage(message);
		session.close();

		// Remove slice csv files.
		if (summaries.size())
		{
			cout << local_time() << "Removing slice csv files" << endl;
			for (size_t s = 0; s < num_slices; ++s)
			{
				const auto slice_csv_path = job_path / (lexical_cast<string>(s) + ".csv");
				remove(slice_csv_path);
			}
		}

		if (phase2only) break;
	}
}

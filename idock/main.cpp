/*

	Copyright (c) 2012, The Chinese University of Hong Kong

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

*/

#include <iostream>
#include <iomanip>
#include <ctime>
#include <boost/thread/thread.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <mongo/client/dbclient.h>
#include <Poco/Net/MailMessage.h>
#include <Poco/Net/MailRecipient.h>
#include <Poco/Net/SMTPClientSession.h>
#include "receptor.hpp"
#include "ligand.hpp"
#include "thread_pool.hpp"
#include "grid_map_task.hpp"
#include "monte_carlo_task.hpp"
#include "summary.hpp"

int main(int argc, char* argv[])
{
	using std::cout;
	using std::string;
	using boost::array;
	using boost::filesystem::path;
	using boost::filesystem::ifstream;
	using boost::filesystem::ofstream;
	using boost::thread;
	using boost::bind;
	using namespace idock;

	// Daemonize itself, changing the current working directory to / and redirecting stdin, stdout and stderr to /dev/null.
//	daemon(0, 0);
	cout << "idock 1.5\n";

	// Fetch command line arguments.
	const auto host = argv[1];
	const auto user = argv[2];
	const auto pwd = argv[3];
	const auto jobs_path = argv[4];

	using namespace mongo;
	DBClientConnection conn;
	{
		// Connect to host and authenticate user.
//		syslog(LOG_INFO, "Connecting to %s and authenticating %s", host, user);
		string errmsg;
		if ((!conn.connect(host, errmsg)) || (!conn.auth("istar", user, pwd, errmsg)))
		{
//			syslog(LOG_ERR, errmsg.c_str());
			return 1;
		}
	}
	const auto collection = "istar.idock";

	// Initialize default values of constant arguments.
	const path ligands_path = "16_lig.pdbqt";
	const path headers_path = "16_hdr.bin";
	const path log1_path = "log1.csv"; // Phase 1 log
	const path log2_path = "log2.csv"; // Phase 2 log
	const path output_folder_path = "output";
	const size_t num_ligands = 12171187;
	const size_t num_threads = thread::hardware_concurrency();
	const size_t seed = time(0);
	const size_t num_mc_tasks = 32;
	const size_t max_conformations = 100;
	const size_t max_results = 20; // Maximum number of results obtained from a single Monte Carlo task.
	const size_t slices[101] = { 0, 121712, 243424, 365136, 486848, 608560, 730272, 851984, 973696, 1095408, 1217120, 1338832, 1460544, 1582256, 1703968, 1825680, 1947392, 2069104, 2190816, 2312528, 2434240, 2555952, 2677664, 2799376, 2921088, 3042800, 3164512, 3286224, 3407936, 3529648, 3651360, 3773072, 3894784, 4016496, 4138208, 4259920, 4381632, 4503344, 4625056, 4746768, 4868480, 4990192, 5111904, 5233616, 5355328, 5477040, 5598752, 5720464, 5842176, 5963888, 6085600, 6207312, 6329024, 6450736, 6572448, 6694160, 6815872, 6937584, 7059296, 7181008, 7302720, 7424432, 7546144, 7667856, 7789568, 7911280, 8032992, 8154704, 8276416, 8398128, 8519840, 8641552, 8763264, 8884976, 9006688, 9128400, 9250112, 9371824, 9493536, 9615248, 9736960, 9858672, 9980384, 10102096, 10223808, 10345520, 10467232, 10588944, 10710655, 10832366, 10954077, 11075788, 11197499, 11319210, 11440921, 11562632, 11684343, 11806054, 11927765, 12049476, 12171187 };
	const fl energy_range = 3.0;
	const fl grid_granularity = 0.08;
	const auto epoch = boost::gregorian::date(1970, 1, 1);

	// Initialize a Mersenne Twister random number generator.
	cout << "Using random seed " << seed << '\n';
	mt19937eng eng(seed);

	// Initialize a thread pool and create worker threads for later use.
	cout << "Creating a thread pool of " << num_threads << " worker threads\n";
	thread_pool tp(num_threads);

	// Precalculate the scoring function in parallel.
	cout << "Precalculating scoring function in parallel\n";
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
		const size_t num_sf_tasks = ((XS_TYPE_SIZE + 1) * XS_TYPE_SIZE) >> 1;
		ptr_vector<packaged_task<void>> sf_tasks(num_sf_tasks);
		for (size_t t1 =  0; t1 < XS_TYPE_SIZE; ++t1)
		for (size_t t2 = t1; t2 < XS_TYPE_SIZE; ++t2)
		{
			sf_tasks.push_back(new packaged_task<void>(bind<void>(&scoring_function::precalculate, boost::ref(sf), t1, t2, boost::cref(rs))));
		}
		BOOST_ASSERT(sf_tasks.size() == num_sf_tasks);

		// Run the scoring function tasks in parallel asynchronously.
		tp.run(sf_tasks);

		// Wait until all the scoring function tasks are completed.
		tp.sync();
	}

	// Precalculate alpha values for determining step size in BFGS.
	array<fl, num_alphas> alphas;
	alphas[0] = 1;
	for (size_t i = 1; i < num_alphas; ++i)
	{
		alphas[i] = alphas[i - 1] * 0.1;
	}

	// Reserve space for containers.
	string line;
	line.reserve(80);
	vector<array3d<fl>> grid_maps(XS_TYPE_SIZE);
	ptr_vector<packaged_task<void>> gm_tasks;
	ptr_vector<packaged_task<void>> mc_tasks(num_mc_tasks);
	ptr_vector<ptr_vector<result>> result_containers;
	result_containers.resize(num_mc_tasks);
	for (auto& rc : result_containers)
	{
		rc.reserve(max_results);
	}
	ptr_vector<result> results;
	results.reserve(max_results * num_mc_tasks);
	vector<size_t> atom_types_to_populate;
	atom_types_to_populate.reserve(XS_TYPE_SIZE);

	// Open files for reading.
	ifstream headers(headers_path);
	ifstream ligands(ligands_path);

	while (true)
	{
		// Fetch a job.
		using namespace bson;
		auto cursor = conn.query(collection, QUERY("scheduled" << BSON("$lt" << 100)).sort("submitted"), 1); // nToReturn = 1
		if (!cursor->more())
		{
			// Sleep for a second.
			using boost::this_thread::sleep_for;
			using boost::chrono::seconds;
			sleep_for(seconds(1));
			continue;
		}

		const auto job = cursor->next();
		const auto _id = job["_id"].OID();
		const auto slice = job["scheduled"].Int();
		const auto slice_str = "slice" + lexical_cast<string>(slice);

		// Execute the job slice.
		cout << "Executing job " << _id << ", slice " << slice << '\n';
		conn.update(collection, BSON("_id" << _id << "$atomic" << 1), BSON("$inc" << BSON("scheduled" << 1)));
		const auto err = conn.getLastError();
		if (!err.empty())
		{
			cerr << err << '\n';
		}

		const path job_path = jobs_path / _id.str();
		const path slice_path = job_path / (lexical_cast<string>(slice) + ".csv");
		const auto start_lig = slices[slice];
		const auto end_lig = slices[slice + 1];
		const auto center_x = job["center_x"].Number();
		const auto center_y = job["center_y"].Number();
		const auto center_z = job["center_z"].Number();
		const auto size_x = job["size_x"].Int();
		const auto size_y = job["size_y"].Int();
		const auto size_z = job["size_z"].Int();
		const auto mwt_lb = job["mwt_lb"].Number();
		const auto mwt_ub = job["mwt_ub"].Number();
		const auto logp_lb = job["logp_lb"].Number();
		const auto logp_ub = job["logp_ub"].Number();
		const auto ad_lb = job["ad_lb"].Number();
		const auto ad_ub = job["ad_ub"].Number();
		const auto pd_lb = job["pd_lb"].Number();
		const auto pd_ub = job["pd_ub"].Number();
		const auto hbd_lb = job["hbd_lb"].Int();
		const auto hbd_ub = job["hbd_ub"].Int();
		const auto hba_lb = job["hba_lb"].Int();
		const auto hba_ub = job["hba_ub"].Int();
		const auto tpsa_lb = job["tpsa_lb"].Int();
		const auto tpsa_ub = job["tpsa_ub"].Int();
		const auto charge_lb = job["charge_lb"].Int();
		const auto charge_ub = job["charge_ub"].Int();
		const auto nrb_lb = job["nrb_lb"].Int();
		const auto nrb_ub = job["nrb_ub"].Int();

		// Initialize the search space of cuboid shape.
		const box b(vec3(center_x, center_y, center_z), vec3(size_x, size_y, size_z), grid_granularity);

		// Parse the receptor.
		cout << "Parsing receptor\n";
		const receptor rec(job["receptor"].String());

		// Divide the box into coarse-grained partitions for subsequent grid map creation.
		array3d<vector<size_t>> partitions(b.num_partitions);
		{
			// Find all the heavy receptor atoms that are within 8A of the box.
			vector<size_t> receptor_atoms_within_cutoff;
			receptor_atoms_within_cutoff.reserve(rec.atoms.size());
			const size_t num_rec_atoms = rec.atoms.size();
			for (size_t i = 0; i < num_rec_atoms; ++i)
			{
				const atom& a = rec.atoms[i];
				if (b.within_cutoff(a.coordinate))
				{
					receptor_atoms_within_cutoff.push_back(i);
				}
			}
			const size_t num_receptor_atoms_within_cutoff = receptor_atoms_within_cutoff.size();

			// Allocate each nearby receptor atom to its corresponding partition.
			for (size_t x = 0; x < b.num_partitions[0]; ++x)
			for (size_t y = 0; y < b.num_partitions[1]; ++y)
			for (size_t z = 0; z < b.num_partitions[2]; ++z)
			{
				partitions(x, y, z).reserve(num_receptor_atoms_within_cutoff);
				const array<size_t, 3> index1 = {{ x,     y,     z     }};
				const array<size_t, 3> index2 = {{ x + 1, y + 1, z + 1 }};
				const vec3 corner1 = b.partition_corner1(index1);
				const vec3 corner2 = b.partition_corner1(index2);
				for (size_t l = 0; l < num_receptor_atoms_within_cutoff; ++l)
				{
					const size_t i = receptor_atoms_within_cutoff[l];
					if (b.within_cutoff(corner1, corner2, rec.atoms[i].coordinate))
					{
						partitions(x, y, z).push_back(i);
					}
				}
			}
		}

		// Resize storage for task containers.
		const size_t num_gm_tasks = b.num_probes[0];
		gm_tasks.reserve(num_gm_tasks);

		// Perform phase 1 screening.
		cout << "Running " << num_mc_tasks << " Monte Carlo tasks per ligand\n";

		// Initialize slice csv. TODO: use bin instead of csv, one size_t for ZINC ID and one float for free energy.
		ofstream csv(slice_path);
		csv.setf(std::ios::fixed, std::ios::floatfield);
		csv << '\n' << std::setprecision(3);

		unsigned int num_completed_ligands = 0;
		headers.seekg(sizeof(size_t) * start_lig);
		for (size_t i = start_lig; i < end_lig; ++i)
		{
			// Locate a ligand.
			size_t header;
			headers.read((char*)&header, sizeof(size_t));
			ligands.seekg(header);

			// Check if the ligand satisfies the filtering conditions.
			getline(ligands, line);
			const auto mwt = right_cast<fl>(line, 21, 28);
			const auto logp = right_cast<fl>(line, 30, 37);
			const auto ad = right_cast<fl>(line, 39, 46);
			const auto pd = right_cast<fl>(line, 48, 55);
			const auto hbd = right_cast<int>(line, 57, 59);
			const auto hba = right_cast<int>(line, 61, 63);
			const auto tpsa = right_cast<int>(line, 65, 67);
			const auto charge = right_cast<int>(line, 69, 71);
			const auto nrb = right_cast<int>(line, 73, 75);
			if (!((mwt_lb <= mwt) && (mwt <= mwt_ub) && (logp_lb <= logp) && (logp <= logp_ub) && (ad_lb <= ad) && (ad <= ad_ub) && (pd_lb <= pd) && (pd <= pd_ub) && (hbd_lb <= hbd) && (hbd <= hbd_ub) && (hba_lb <= hba) && (hba <= hba_ub) && (tpsa_lb <= tpsa) && (tpsa <= tpsa_ub) && (charge_lb <= charge) && (charge <= charge_ub) && (nrb_lb <= nrb) && (nrb <= nrb_ub))) continue;

			// Obtain ligand ID. ZINC IDs are 8-character long.
			const auto lig_id = line.substr(10, 8);

			// Parse the ligand.
			cout << "Parsing ligand " << lig_id << '\n';
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
			const size_t num_atom_types_to_populate = atom_types_to_populate.size();
			if (num_atom_types_to_populate)
			{
				cout << "Creating " << std::setw(2) << num_atom_types_to_populate << " grid maps\n";
				BOOST_ASSERT(gm_tasks.empty());
				for (size_t x = 0; x < num_gm_tasks; ++x)
				{
					gm_tasks.push_back(new packaged_task<void>(bind<void>(grid_map_task, boost::ref(grid_maps), boost::cref(atom_types_to_populate), x, boost::cref(sf), boost::cref(b), boost::cref(rec), boost::cref(partitions))));
				}
				tp.run(gm_tasks);
				for (auto& t : gm_tasks)
				{
					t.get_future().get();
				}
				tp.sync();
				gm_tasks.clear();
				atom_types_to_populate.clear();
			}

			// Run Monte Carlo tasks.
			cout << "Running " << num_mc_tasks << " Monte Carlo tasks\n";
			BOOST_ASSERT(mc_tasks.empty());
			for (size_t i = 0; i < num_mc_tasks; ++i)
			{
				BOOST_ASSERT(result_containers[i].empty());
				mc_tasks.push_back(new packaged_task<void>(bind<void>(monte_carlo_task, boost::ref(result_containers[i]), boost::cref(lig), eng(), boost::cref(alphas), boost::cref(sf), boost::cref(b), boost::cref(grid_maps))));
			}
			tp.run(mc_tasks);

			// Merge results from all the tasks into one single result container.
			BOOST_ASSERT(results.empty());
			const fl required_square_error = static_cast<fl>(4 * lig.num_heavy_atoms); // Ligands with RMSD < 2.0 will be clustered into the same cluster.
			for (size_t i = 0; i < num_mc_tasks; ++i)
			{
				mc_tasks[i].get_future().get();
				ptr_vector<result>& task_results = result_containers[i];
				const size_t num_task_results = task_results.size();
				for (size_t j = 0; j < num_task_results; ++j)
				{
					add_to_result_container(results, static_cast<result&&>(task_results[j]), required_square_error);
				}
				task_results.clear();
			}

			// Block until all the Monte Carlo tasks are completed.
			tp.sync();
			mc_tasks.clear();

			// If no conformation can be found, skip the current ligand and proceed with the next one.
			if (results.empty()) continue; // Possible if and only if results.size() == 0 because max_conformations >= 1 is enforced when parsing command line arguments.
			const size_t num_results = std::min<size_t>(results.size(), max_conformations);

			// Adjust free energy relative to flexibility.
			result& best_result = results.front();
			best_result.e_nd = best_result.f * lig.flexibility_penalty_factor;

			// Clear the results of the current ligand.
			results.clear();

			// Dump ligand summaries to the csv file.
			cout << lig_id << ',' << best_result.e_nd << '\n';

			// Refresh progress every 32 ligands.
			if (!(++num_completed_ligands & 32))
			{
				cout << "Current progress " << num_completed_ligands << '\n';
				conn.update(collection, BSON("_id" << _id << "$atomic" << 1), BSON("$set" << BSON(slice_str << num_completed_ligands)));
			}
		}
		csv.close();

		// Increment the completed indicator.
		conn.update(collection, BSON("_id" << _id << "$atomic" << 1), BSON("$set" << BSON(slice_str << num_completed_ligands)));
		conn.update(collection, BSON("_id" << _id << "$atomic" << 1), BSON("$inc" << BSON("completed" << 1)));

		// If all the slices are done, perform phase 2 screening.
		if (!conn.query(collection, QUERY("_id" << _id << "completed" << 100), 1)->more()) continue;

		// Initialize necessary variables for storing ligand summaries.
		ptr_vector<summary> summaries(num_ligands);

		// Combine multiple csv's.
		for (size_t s = 0; s < 100; ++s)
		{
			const auto csv_path = job_path / (lexical_cast<string>(s) + ".csv");
			ifstream in(csv_path);
			while (getline(in, line))
			{
				summaries.push_back(new summary(line.substr(0, 8), lexical_cast<fl>(line.substr(9))));
			}
			// Delete csv
			remove(csv_path);
		}

		// Sort the summaries.
		summaries.sort();

		// Save summaries.
		{
			ofstream log1(log1_path);
			for (const auto& s : summaries)
			{
				log1 << s.lig_id << ',' << s.energy << '\n';
			}
		}

		// Phase 2

		// Update progress
		using boost::chrono::system_clock;
		using boost::chrono::duration_cast;
		using chrono_millis = boost::chrono::milliseconds;
		const auto millis_since_epoch = duration_cast<chrono_millis>(system_clock::now().time_since_epoch()).count();
		conn.update(collection, BSON("_id" << _id), BSON("$set" << BSON("done" << Date_t(millis_since_epoch))));

		// Send completion notification email.
		const auto email = job["email"].String();
		cout << "Sending a completion notification email to " << email << '\n';
		using Poco::Net::MailMessage;
		using Poco::Net::MailRecipient;
		using Poco::Net::SMTPClientSession;
		MailMessage message;
		message.setSender("idock <noreply@cse.cuhk.edu.hk>");
		message.setSubject("Your idock job has completed");
		using boost::posix_time::ptime;
		using posix_millis = boost::posix_time::milliseconds;
		using boost::posix_time::to_simple_string;
		message.setContent("Your idock job submitted on " + to_simple_string(ptime(epoch, posix_millis(job["submitted"].Date().millis))) + " UTC docking " + lexical_cast<string>(job["ligands"].Int()) + " ligands with description as \"" + job["description"].String() + "\" was done on " + to_simple_string(ptime(epoch, posix_millis(millis_since_epoch))) + " UTC. View result at http://idock.cse.cuhk.edu.hk");
		message.addRecipient(MailRecipient(MailRecipient::PRIMARY_RECIPIENT, email));
		SMTPClientSession session("137.189.91.190");
		session.login();
		session.sendMessage(message);
		session.close();
	}
}

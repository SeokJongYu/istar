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

#include <syslog.h>
#include <sstream>
#include <string>
#include <vector>
#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <mongo/client/dbclient.h>
#include <cuda_runtime_api.h>
#include <Poco/Net/MailMessage.h>
#include <Poco/Net/MailRecipient.h>
#include <Poco/Net/SMTPClientSession.h>

using std::string;
using std::vector;
using boost::filesystem::path;
using boost::filesystem::directory_iterator;

#define CHARACTER_CARDINALITY 4	/**< One nucleotide is either A, C, G, or T. */
#define MAX_UNSIGNED_INT	0xffffffffUL	/**< The maximum value of an unsigned int. */
#define MAX_UNSIGNED_LONG_LONG	0xffffffffffffffffULL	/**< The maximum value of an unsigned long long. */
#define B 7	/**< Each thread block consists of 2^B (=1<<B) threads. */
#define L 8	/**< Each thread processes 2^L (=1<<L) special codons plus those in the overlapping zone of two consecutive threads. */
// Since each thread block processes 1 << (L + B) special codons, the number of thread blocks will be up to (MAX_SCODON_COUNT + 1 << (L + B) - 1) >> (L + B).
// This program uses 1D CUDA thread organization, so at most 65,536 threads can be specified.
// Therefore, the inequation ((MAX_SCODON_COUNT + (1 << (L + B)) - 1) >> (L + B)) <= 65,536 must hold.
// MAX_SCODON_COUNT = 0.22G ==> L + B >= 12 is required.

/**
 * Transfer necessary parameters to CUDA constant memory.
 * This agrep kernel initialization should be called only once for searching the same corpus.
 * @param[in] scodon_arg The special codon array.
 * @param[in] character_count_arg Actual number of characters.
 * @param[in] match_arg The match array.
 * @param[in] max_match_count_arg Maximum number of matches of one single query.
 */
extern "C" void initAgrepKernel(const unsigned int *scodon_arg, const unsigned int character_count_arg, const unsigned int *match_arg, const unsigned int max_match_count_arg);

/**
 * Transfer 32-bit mask array and test bit from host to CUDA constant memory.
 * @param[in] mask_array_arg The mask array of a pattern.
 * @param[in] test_bit_arg The test bit.
 */
extern "C" void transferMaskArray32(const unsigned int *mask_array_arg, const unsigned int test_bit_arg);

/**
 * Transfer 64-bit mask array and test bit from host to CUDA constant memory.
 * @param[in] mask_array_arg The mask array of a pattern.
 * @param[in] test_bit_arg The test bit.
 */
extern "C" void transferMaskArray64(const unsigned long long *mask_array_arg, const unsigned long long test_bit_arg);

/**
 * Invoke the CUDA implementation of agrep kernel.
 * @param[in] m Pattern length.
 * @param[in] k Edit distance.
 * @param[in] block_count Number of thread blocks.
 */
extern "C" void invokeAgrepKernel(const unsigned int m, const unsigned int k, const unsigned int block_count);

/**
 * Get the number of matches from CUDA constant memory.
 * @param[out] match_count_arg Number of matches.
 */
extern "C" void getMatchCount(unsigned int *match_count_arg);

/**
 * Syslog and exit if a CUDA runtime API error occurs.
 * @param[in] err CUDA runtime API error returned by CUDA functions.
 */
inline void cudaSyslog(const cudaError err)
{
	if (cudaSuccess != err)
	{
		syslog(LOG_ERR, "cudaSyslog() Runtime API error %d: %s.", err, cudaGetErrorString(err));
		exit(-1);
	}
}

/**
 * Encode a character to its 2-bit binary representation.
 * The last two but one bits are different for A, C, G, and T respectively.
 * Note that some genomes contain 'N', which will be treated as 'G' in this encoding function.
 *
 * 'A' = 65 = 01000<b>00</b>1
 *
 * 'C' = 67 = 01000<b>01</b>1
 *
 * 'G' = 71 = 01000<b>11</b>1
 *
 * 'N' = 78 = 01001<b>11</b>0
 *
 * 'T' = 84 = 01010<b>10</b>0
 * @param[in] character The character to be encoded.
 * @return The 2-bit binary representation of given character.
 */
inline unsigned int encode(char character)
{
	return (toupper(character) >> 1) & 3;
}

/// Represents a genome in FASTA format.
class genome
{
public:
	const unsigned int taxon;	/**< Taxonomy ID. */
	string name;	/**< Genome name. */
	const unsigned int sequence_count;	/**< Actual number of sequences. */
	const unsigned int character_count;	/**< Actual number of characters. */
	vector<string> sequence_header;	/**< Headers of sequences. */
	vector<unsigned int> sequence_length;	/**< Lengthes of sequences. */
	vector<unsigned int> sequence_cumulative_length;	/**< Cumulative lengths of sequences, i.e. 1) sequence_cumulative_length[0] = 0; 2) sequence_cumulative_length[sequence_index + 1] = sequence_cumulative_length[sequence_index] + sequence_length[sequence_index]; */
	const unsigned int scodon_count;	/**< Actual number of special codons. */
	const unsigned int block_count;	/**< Actual number of thread blocks. */
	vector<unsigned int> scodon;	/**< The entire genomic nucleotides are stored into this array, one element of which, i.e. one 32-bit unsigned int, can store up to 16 nucleotides because one nucleotide can be uniquely represented by two bits since it must be either A, C, G, or T. One unsigned int is called a special codon, or scodon for short, because it is similar to codon, in which three consecutive characters of mRNA determine one amino acid of resulting protein. */
	vector<unsigned int> block_to_sequence;	/**< Mapping of thread blocks to sequences. */

	/**
	 * Construct a genome by loading its FASTA files.
	 * @param[in] taxon Taxonomy ID, e.g. 9606 for human.
	 * @param[in] name Scientific name followed by common name in brackets, e.g. Homo sapiens (Human).
	 * @param[in] sequence_count Number of sequences. For assembled genomes, it equals the number of FASTA files.
	 * @param[in] character_count Number of characters.
	 */
	explicit genome(const unsigned int taxon, const string name, const unsigned int sequence_count, const unsigned int character_count) :
		taxon(taxon),
		name(name),
		sequence_count(sequence_count),
		character_count(character_count),
		sequence_header(sequence_count),
		sequence_length(sequence_count),
		sequence_cumulative_length(sequence_count + 1),
		scodon_count((character_count + 16 - 1) >> 4),
		block_count((scodon_count + (1 << (L + B)) - 1) >> (L + B)),
		scodon(block_count << (L + B)),
		block_to_sequence(block_count)
	{
		sequence_cumulative_length[0] = 0;

		syslog(LOG_INFO, "Loading the genome of %s", name.c_str());
		unsigned int scodon_buffer = 0;	// 16 consecutive characters will be accommodated into one 32-bit unsigned int.
		unsigned int scodon_index;	// scodon[scodon_index] = scodon_buffer; In CUDA implementation, special codons need to be properly shuffled in order to satisfy coalesced global memory access.
		int sequence_index = -1;	// Index of the current sequence.
		unsigned int character_index = 0;	// Index of the current character across all the sequences of the entire genome.
		string line;
		line.reserve(1000);
		const directory_iterator dir_iter;
		for (directory_iterator di(name); di != dir_iter; ++di)
		{
			using boost::filesystem::ifstream;
			ifstream in(di->path());
			while (getline(in, line))
			{
				if (line.front() == '>') // Header line
				{
					if (++sequence_index > 0) // Not the first sequence.
					{
						sequence_cumulative_length[sequence_index] = character_index;
						sequence_length[sequence_index - 1] = character_index - sequence_cumulative_length[sequence_index - 1];
					}
					sequence_header[sequence_index] = line;
				}
				else
				{
					for (const auto c : line)
					{
						const unsigned int character_index_lowest_four_bits = character_index & 15;
						scodon_buffer |= encode(c) << (character_index_lowest_four_bits << 1); // Earlier characters reside in lower bits, while later characters reside in higher bits.
						if (character_index_lowest_four_bits == 15) // The buffer is full. Flush it to the special codon array. Note that there is no need to clear scodon_buffer.
						{
							scodon_index = character_index >> 4;
							// scodon_index can be splitted into 3 parts:
							// scodon_index = block_index << (L + B) | thread_index << L | scodon_index;
							// because 1) each thread block processes 1 << (L + B) special codons,
							//     and 2) each thread processes 1 << L special codons.
							// The scodon_index is accommodated in the lowest L bits.
							// The thread_index is accommodated in the middle B bits.
							// The block_index  is accommodated in the highest 32 - (L + B) bits.
							// This program uses 1D CUDA thread organization, so at most 65,536 threads can be specified, i.e. at least 16 bits should be reserved for block_index.
							// Therefore, the inequation 32 - (L + B) >= 16 must hold. ==> L + B <= 16.
							// In order to satisfy coalesced global memory access, thread_index should be rearranged to the lowest B bits.
							// To achieve this goal,
							//         1) block_index remains at the highest 32 - (L + B) bits,
							//   while 2) thread_index should be rearranged to the lowest B bits,
							//     and 3) scodon_index should be rearranged to the middle L bits.
							// Finally, scodon_index = block_index << (L + B) | scodon_index << B | thread_index;
							scodon_index = (scodon_index & (MAX_UNSIGNED_INT ^ ((1 << (L + B)) - 1)))
										 | ((scodon_index & ((1 << L) - 1)) << B)
										 | ((scodon_index >> L) & ((1 << B) - 1));
							scodon[scodon_index] = scodon_buffer;
							scodon_buffer = 0;
						}
						++character_index;
					}
				}
			}
		}
		BOOST_ASSERT(character_count == character_index);
		BOOST_ASSERT(sequence_count == sequence_index + 1);

		// Calculate statistics.
		sequence_cumulative_length[sequence_count] = character_count;
		sequence_length[sequence_index] = character_count - sequence_cumulative_length[sequence_index];
		scodon_index = character_index >> 4;
		if (scodon_index < scodon_count)	// There are some nucleotides in the special codon buffer, flush it.
		{
			scodon_index = (scodon_index & (MAX_UNSIGNED_INT ^ ((1 << (L + B)) - 1)))
						 | ((scodon_index & ((1 << L) - 1)) << B)
						 | ((scodon_index >> L) & ((1 << B) - 1));
			scodon[scodon_index] = scodon_buffer; // Now the last special codon might have zeros in its least significant bits. Don't treat such zeros as 'A's.
		}

		// Calculate thread block to sequence index mapping.
		for (unsigned int block = 0, character = 0, sequence = 0; block < block_count; ++block)
		{
			while (character >= sequence_cumulative_length[sequence + 1]) ++sequence;
			block_to_sequence[block] = sequence;
			character += (1 << (L + B + 4)); // One thread block processes 1 << (L + B) special codons, and each special codon encodes 1 << 4 characters.
		}
	}

	/// Default copy constructor.
	genome(const genome&) = default;
	/// Default move constructor.
	genome(genome&&) = default;
	/// Default copy assignment operator.
	genome& operator=(const genome&) = default;
	/// Default move assignment operator.
	genome& operator=(genome&&) = default;
};

int main(int argc, char** argv)
{
	// Daemonize itself, retaining the current working directory and redirecting stdin, stdout and stderr to /dev/null.
	daemon(1, 0);
	syslog(LOG_INFO, "igrep 1.0");

	string host, db, user, pwd;
	path jobs_path;
	{
		// Create program options.
		using namespace boost::program_options;
		options_description options("input (required)");
		options.add_options()
			("host", value<string>(&host)->required(), "server to connect to")
			("db"  , value<string>(&db  )->required(), "database to login to")
			("user", value<string>(&user)->required(), "username for authentication")
			("pwd" , value<string>(&pwd )->required(), "password for authentication")
			("jobs", value<path>(&jobs_path)->required(), "path to jobs directory")
			;

		// If no command line argument is supplied, simply exit.
		if (argc == 1) return 0;

		// Parse command line arguments.
		variables_map vm;
		store(parse_command_line(argc, argv, options), vm);
		vm.notify();
	}

	using namespace mongo;
	DBClientConnection conn;
	{
		// Connect to host and authenticate user.
		syslog(LOG_INFO, "Connecting to %s and authenticating %s", host.c_str(), user.c_str());
		string errmsg;
		if ((!conn.connect(host, errmsg)) || (!conn.auth(db, user, pwd, errmsg)))
		{
			syslog(LOG_ERR, errmsg.c_str());
			return 1;
		}
	}
	const auto collection = db + ".igrep";

	// Initialize genomes.
	vector<genome> genomes;
	genomes.reserve(17);
	genomes.push_back(genome(13616, "Monodelphis domestica (Gray short-tailed opossum)", 9, 3502373038));
	genomes.push_back(genome(9598, "Pan troglodytes (Chimpanzee)", 25, 3175582169));
	genomes.push_back(genome(9606, "Homo sapiens (Human)", 24, 3095677412));
	genomes.push_back(genome(9544, "Macaca mulatta (Rhesus monkey)", 21, 2863665185));
	genomes.push_back(genome(10116, "Rattus norvegicus (Rat)", 21, 2718881021));
	genomes.push_back(genome(10090, "Mus musculus (Mouse)", 21, 2654895218));
	genomes.push_back(genome(9913, "Bos taurus (Cow)", 30, 2634413324));
	genomes.push_back(genome(9615, "Canis familiaris (Dog)", 39, 2445110183));
	genomes.push_back(genome(9796, "Equus caballus (Domestic horse)", 32, 2367053447));
	genomes.push_back(genome(7955, "Danio rerio (Zebrafish)", 25, 1277075233));
	genomes.push_back(genome(9031, "Gallus gallus (Chicken)", 31, 1031883471));
	genomes.push_back(genome(59729, "Taeniopygia guttata (Zebra finch)", 34, 1018092713));
	genomes.push_back(genome(9823, "Sus scrofa (Pig)", 10, 813033904));
	genomes.push_back(genome(9258, "Ornithorhynchus anatinus (Platypus)", 19, 437080024));
	genomes.push_back(genome(29760, "Vitis vinifera (Grape)", 19, 303085820));
	genomes.push_back(genome(7460, "Apis mellifera (Honey bee)", 16, 217194876));
	genomes.push_back(genome(7070, "Tribolium castaneum (Red flour beetle)", 10, 187494969));

	// Declare kernel variables.
	unsigned int       mask_array_32[CHARACTER_CARDINALITY];	// The 32-bit mask array of pattern.
	unsigned long long mask_array_64[CHARACTER_CARDINALITY];	// The 64-bit mask array of pattern.
	unsigned int       test_bit_32;	// The test bit for determining matches of patterns of length 32.
	unsigned long long test_bit_64;	// The test bit for determining matches of patterns of length 64.
	const unsigned int max_match_count = 1000;	// Maximum number of matches of one single query.
	unsigned int match[max_match_count];	// The matches returned by the CUDA agrep kernel.
	unsigned int match_count;	// Actual number of matches in the match array. match_count <= potential_match_count should always holds.
	unsigned int *scodon_device;	// CUDA global memory pointer pointing to the special codon array.
	unsigned int *match_device;	// CUDA global memory pointer pointing to the match array.

	while (true)
	{
		// Fetch jobs.
		using namespace bson;
		auto cursor = conn.query(collection, QUERY("done" << BSON("$exists" << false)).sort("submitted"), 100); // Each batch processes 100 jobs.
		while (cursor->more())
		{
			const auto job = cursor->next();
			const auto _id = job["_id"].OID();
			syslog(LOG_INFO, "Executing job %s", _id.str().c_str());

			// Obtain the target genome via taxon.
			const auto taxon = job["genome"].Int();
			unsigned int i;
			for (i = 0; i < genomes.size(); ++i)
			{
				if (taxon == genomes[i].taxon) break;
			}
			BOOST_ASSERT(i < genomes.size());
			const auto& g = genomes[i];
			syslog(LOG_INFO, "Searching the genome of %s", g.name.c_str());

			// Set up CUDA kernel.
			cudaSyslog(cudaMalloc((void**)&scodon_device, sizeof(unsigned int) * g.scodon.size()));
			cudaSyslog(cudaMemcpy(scodon_device, &g.scodon.front(), sizeof(unsigned int) * g.scodon.size(), cudaMemcpyHostToDevice));
			cudaSyslog(cudaMalloc((void**)&match_device, sizeof(unsigned int) * max_match_count));
			initAgrepKernel(scodon_device, g.character_count, match_device, max_match_count);

			// Create a job directory, open log and pos files, and write headers.
			const path job_path = jobs_path / _id.str();
			create_directory(job_path);
			using boost::filesystem::ofstream;
			ofstream log(job_path / "log.csv");
			ofstream pos(job_path / "pos.csv");
			log << "Query Index,Pattern,Edit Distance,Number of Matches\n";
			pos << "Query Index,Match Index,Sequence Index,Ending Position\n";

			// Parse and execute queries.
			using std::istringstream;
			istringstream in(job["queries"].String());
			string line;
			for (unsigned int qi = 0; getline(in, line); ++qi)
			{
				BOOST_ASSERT(line.size() <= 65);
				const unsigned int m = line.size() - 1;		// Pattern length.
				const unsigned int k = line.back() - 48;	// Edit distance.
				if (m <= 32)
				{
					memset(mask_array_32, 0, sizeof(unsigned int) * CHARACTER_CARDINALITY);
					for (unsigned int i = 0; i < m; ++i)
					{
						unsigned int j = (unsigned int)1 << i;
						if ((line[i] == 'N') || (line[i] == 'n'))
						{
							mask_array_32[0] |= j;
							mask_array_32[1] |= j;
							mask_array_32[2] |= j;
							mask_array_32[3] |= j;
						}
						else
						{
							mask_array_32[encode(line[i])] |= j;
						}
					}
					mask_array_32[0] ^= MAX_UNSIGNED_INT;
					mask_array_32[1] ^= MAX_UNSIGNED_INT;
					mask_array_32[2] ^= MAX_UNSIGNED_INT;
					mask_array_32[3] ^= MAX_UNSIGNED_INT;
					test_bit_32 = (unsigned int)1 << (m - 1);
					transferMaskArray32(mask_array_32, test_bit_32);
				}
				else // m > 32
				{
					memset(mask_array_64, 0, sizeof(unsigned long long) * CHARACTER_CARDINALITY);
					for (unsigned int i = 0; i < m; ++i)	// Derive the mask array of current pattern.
					{
						unsigned long long j = (unsigned long long)1 << i;
						if ((line[i] == 'N') || (line[i] == 'n'))
						{
							mask_array_64[0] |= j;
							mask_array_64[1] |= j;
							mask_array_64[2] |= j;
							mask_array_64[3] |= j;
						}
						else
						{
							mask_array_64[encode(line[i])] |= j;
						}
					}
					mask_array_64[0] ^= MAX_UNSIGNED_LONG_LONG;
					mask_array_64[1] ^= MAX_UNSIGNED_LONG_LONG;
					mask_array_64[2] ^= MAX_UNSIGNED_LONG_LONG;
					mask_array_64[3] ^= MAX_UNSIGNED_LONG_LONG;
					test_bit_64 = (unsigned long long)1 << (m - 1);
					transferMaskArray64(mask_array_64, test_bit_64);
				}

				// Invoke kernel.
				invokeAgrepKernel(m, k, g.block_count);
				cudaSyslog(cudaGetLastError());
				// Don't waste the CPU time before waiting for the CUDA agrep kernel to exit.
				const unsigned int m_minus_k = m - k;	// Used to determine whether a match is across two consecutive sequences.
				const unsigned int m_plus_k = m + k;	// Used to determine whether a match is across two consecutive sequences.
				cudaSyslog(cudaDeviceSynchronize());	// Block until the CUDA agrep kernel completes.

				// Retrieve matches from device.
				getMatchCount(&match_count);
				if (match_count > max_match_count) match_count = max_match_count;	// If the number of matches exceeds max_match_count, only the first max_match_count matches will be saved into the result file.
				cudaSyslog(cudaMemcpy(match, match_device, sizeof(unsigned int) * match_count, cudaMemcpyDeviceToHost));

				// Decompose absolute matches into sequences and positions within sequence.
				vector<unsigned int> match_sequences, match_positions;
				match_sequences.reserve(match_count);
				match_positions.reserve(match_count);
				for (unsigned int i = 0; i < match_count; i++)
				{
					unsigned int position = match[i];	// The absolute ending position of current match.
					unsigned int sequence = g.block_to_sequence[position >> (L + B + 4)];	// Use block-to-sequence mapping to get the nearest sequence index.
					while (position >= g.sequence_cumulative_length[sequence + 1]) sequence++; // Now sequence is the sequence index of match[i].
					position -= g.sequence_cumulative_length[sequence];	// Now position is the character index within sequence.
					if (position + 1 < m_minus_k) continue; // The current match must be across two consecutive sequences. It is thus an invalid matching.
					match_sequences.push_back(sequence);
					match_positions.push_back(position);
				}

				// Output filtered matches.
				const auto filtered_match_count = match_sequences.size();
				log << qi << ',' << line.substr(0, m) << ',' << k << ',' << filtered_match_count << '\n';
				for (auto i = 0; i < filtered_match_count; ++i)
				{
					pos << qi << ',' << i << ',' << match_sequences[i] << ',' << match_positions[i] << '\n';
//					if ((match_sequences[i] > 0) && (match_positions[i] + 1 < m_plus_k)); // This match may possibly be across two consecutive sequences.
				}
			}

			// Release resources.
			pos.close();
			log.close();
			cudaSyslog(cudaFree(match_device));
			cudaSyslog(cudaFree(scodon_device));
			cudaSyslog(cudaDeviceReset());

			// Update progress.
			using boost::chrono::system_clock;
			using boost::chrono::duration_cast;
			using boost::chrono::milliseconds;
			conn.update(collection, BSON("_id" << _id), BSON("$set" << BSON("done" << Date_t(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()))));
			const auto err = conn.getLastError();
			if (!err.empty())
			{
				syslog(LOG_ERR, err.c_str());
			}

			// Send completion notification email.
			const auto email = job["email"].String();
			syslog(LOG_INFO, "Sending a completion notification email to %s", email.c_str());
			using Poco::Net::MailMessage;
			using Poco::Net::MailRecipient;
			using Poco::Net::SMTPClientSession;
			MailMessage message;
			message.setSender("igrep <noreply@cse.cuhk.edu.hk>");
			message.setSubject("Your igrep job has completed");
			message.setContent("View result at http://igrep.cse.cuhk.edu.hk");
			message.addRecipient(MailRecipient(MailRecipient::PRIMARY_RECIPIENT, email));
			SMTPClientSession session("137.189.91.190");
			session.login();
			session.sendMessage(message);
			session.close();
		}

		// Sleep for a second.
		using boost::this_thread::sleep_for;
		using boost::chrono::seconds;
		sleep_for(seconds(1));
	}
	return 0;
}

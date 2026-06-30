#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <inttypes.h>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <climits>
#include <limits>
#include <windows.h>

using namespace std;

// Core optimization constants (tunable for speed/memory balance)
constexpr size_t SIEVE_BLOCK_SIZE = 1ULL << 25ULL;  // 2^25 (33554432) sieve block size (faster bulk generation)
constexpr size_t WRITE_BUFFER_SIZE = 1ULL << 27ULL; // 128 MB write buffer (minimizes syscalls)
const char* filename = "Primes.txt";

bool isStop = false, stopNote = false;

// Segmented Sieve of Eratosthenes: Generates primes in [low, high) as comma-suffixed strings
void segmentedSieve(uint64_t low, uint64_t high, vector<string>& primesOut) {
	// Sieve works on odd numbers only (map x ¡ú (x - low)/2) ¨C handle 2 separately if in range
	if (low <= 2 && high > 2) {
		primesOut.emplace_back("2,");
	}

	// If low is even, start from next odd (avoids even numbers in sieve)
	if ((low & 1) == 0) {
		low++;
	}

	// Calculate sieve size (number of odd numbers in [low, high))
	if (low >= high) {
		return; // No odd numbers in range
	}
	size_t sieveSize = (high - low + 1) / 2;
	vector<bool> isPrime(sieveSize, true);

	// Generate base primes up to sqrt(high) (for marking multiples in the segment)
	uint64_t sqrtHigh = static_cast<uint64_t>(sqrt(static_cast<long double>(high))) + 1;
	vector<uint64_t> basePrimes;
	vector<bool> smallSieve(sqrtHigh, true);
	smallSieve[0] = smallSieve[1] = false;
	for (uint64_t i = 2; i < sqrtHigh; ++i) {
		if (isStop && (!stopNote)) {
			cout << "\nStopping...";
			stopNote = true;
		}
		if (smallSieve[i]) {
			basePrimes.push_back(i);
			for (uint64_t j = i * i; j < sqrtHigh; j += i) {
				smallSieve[j] = false;
			}
		}
	}

	// Mark multiples of base primes in the current segment (only odd primes)
	for (uint64_t p : basePrimes) {
		if (p == 2) continue; // Even numbers already handled (2 is added explicitly)
		if (isStop && (!stopNote)) {
			cout << "\nStopping...";
			stopNote = true;
		}

		// Find first multiple of p ¡Ý low (must be odd, since p is odd and low is odd)
		uint64_t start = max(p * p, ((low + p - 1) / p) * p); // Ceiling division to get first multiple
		// Ensure start is odd (redundant here, but safe-guard)
		if ((start & 1) == 0) {
			start += p;
		}

		// Map start to sieve index: (start - low) / 2 (since low is odd, start - low is even)
		size_t idx = (start - low) / 2;
		// Mark all multiples of p (step by p, since even steps are unnecessary for odd p)
		for (size_t j = idx; j < sieveSize; j += p) {
			isPrime[j] = false;
		}
	}

	// Collect primes from sieve and convert to comma-suffixed strings (bulk processing)
	for (size_t i = 0; i < sieveSize; ++i) {
		if (isStop && (!stopNote)) {
			cout << "\nStopping...";
			stopNote = true;
		}
		if (isPrime[i]) {
			uint64_t prime = low + 2 * i; // Reconstruct original prime (low + 2*i = odd)
			primesOut.emplace_back(to_string(prime) + ",");
		}
	}
}

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
	if (ctrlType == CTRL_C_EVENT) isStop = true;
	return TRUE;
}

void getLastElement(const char* filename, std::string& result) {
	result.clear();

	// Open file in binary mode for precise seeking
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		result.clear();
		return;
	}
	if (!file.good()) {
		result.clear();
		return;
	}

	// Get file size
	std::ifstream::pos_type fileSize = file.tellg();
	if (fileSize == 0 || fileSize == std::ifstream::pos_type(-1)) {
		result.clear();
		return;
	}

	// Start from the end and look for the last comma
	std::ifstream::pos_type currentPos = fileSize - 1;
	int foundCommas = 0;
	std::ifstream::pos_type secondCommaPos = -1;
	std::ifstream::pos_type firstCommaPos = -1;

	char ch = 0;

	// Read backwards from the end
	while (currentPos >= 0 && foundCommas < 2) {
		file.seekg(currentPos);
		file.get(ch);

		if (ch == ',') {
			if (foundCommas == 0) {
				secondCommaPos = currentPos;
			}
			else if (foundCommas == 1) {
				firstCommaPos = currentPos;
			}
			foundCommas++;
		}

		if (currentPos == 0) break; // Prevent underflow
		currentPos = currentPos - 1;
	}

	// Check if we found two commas
	if (foundCommas < 2 || firstCommaPos == -1 || secondCommaPos == -1) {
		result.clear();
		return;
	}

	// Calculate the length of the content between commas
	std::size_t contentLength = static_cast<std::size_t>(secondCommaPos - firstCommaPos - 1);
	if (contentLength == 0) {
		result.clear();
		return;
	}

	// Read the content between the two commas
	result.resize(contentLength);
	file.seekg(firstCommaPos + 1);
	file.read(&result[0], contentLength);

	// Verify the last character is a comma (as per specification)
	if (fileSize > 0) {
		file.seekg(fileSize - 1);
		file.get(ch);
		if (ch != ',') {
			result.clear();
			return;
		}
	}
}

// High-speed prime generator: Starts at 2, continuous bulk generation + buffered I/O
void generatePrimesHighSpeed(const char* outputFile) {
	std::string result;
	getLastElement(outputFile, result);
	ofstream outFile;
	uint64_t currentLow = 2ULL;

	// Open file in binary mode (faster than text mode, no newline conversion)
	if (!result.empty()) {
		currentLow = std::stoull(result);
		outFile.open(outputFile, ios::out | ios::binary | ios::app);
		if (currentLow >= 999999999999999999ULL) {
			outFile.close();
			cout << "\nStopped.\nPress enter to exit.\n";
			cin.ignore();
			return;
		}
		cout << "Warning: Generation continues from " << currentLow << ".\n\n";
	}
	else {
		outFile.open(outputFile, ios::out | ios::binary);
	}
	if (!outFile) {
		cerr << "Error: Could not open output file!\n";
		cout << "Press enter to exit.\n";
		cin.ignore();
		return;
	}
	// Preallocate large write buffer to minimize expensive system calls
	vector<char> writeBuffer(WRITE_BUFFER_SIZE);
	size_t bufferUsed = 0;

	cout << "Generating primes (high-speed)... Press Ctrl+C to stop." << endl;

	while ((!isStop) && currentLow < 999999999999999999ULL) { // Continuous generation
		vector<string> primeBatch;
		// Generate primes in [currentLow, currentLow + SIEVE_BLOCK_SIZE)
		segmentedSieve(currentLow, currentLow + SIEVE_BLOCK_SIZE, primeBatch);

		// Copy batch to write buffer (bulk memory copy = fast)
		for (const string& primeStr : primeBatch) {
			if (isStop && (!stopNote)) {
				cout << "\nStopping...";
				stopNote = true;
			}
			const size_t strLen = primeStr.size();
			// Flush buffer if adding current string would overflow it
			if (bufferUsed + strLen > WRITE_BUFFER_SIZE) {
				outFile.write(writeBuffer.data(), bufferUsed);
				outFile.flush();
				bufferUsed = 0;
			}
			// Copy string data directly to buffer (avoids per-prime I/O)
			memcpy(writeBuffer.data() + bufferUsed, primeStr.data(), strLen);
			bufferUsed += strLen;
		}

		// Move to next sieve block (no overlap with previous)
		currentLow += SIEVE_BLOCK_SIZE;
	}

	// Flush remaining data
	if (bufferUsed > 0) {
		outFile.write(writeBuffer.data(), bufferUsed);
		outFile.flush();
	}

	outFile.close();
	cout << "\nStopped.\nPress enter to exit.\n";
	cin.ignore();
}

int main() {
	static_assert(INT_MAX >= 2147483647ULL && SIZE_MAX >= (1ULL << 32ULL) - 1 && ('0' + 1 == '1' && '1' + 1 == '2' && '2' + 1 == '3' && '3' + 1 == '4' && '4' + 1 == '5' && '5' + 1 == '6' && '6' + 1 == '7' && '7' + 1 == '8' && '8' + 1 == '9') && std::numeric_limits<long double>::digits10 >= 18 && std::numeric_limits<long double>::is_iec559 == true, "Unsupported platform or compiler");

	if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
		return -1;
	}
	if (!SetConsoleTitleA("Primes plus plus")) {
		return -1;
	}

	generatePrimesHighSpeed(filename);

	return 0;
}

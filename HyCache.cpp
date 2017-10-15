// HyCache.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <conio.h>
#include <Windows.h>
#include <ctime>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <climits>
#include <cmath>
using namespace std;

struct fileSizeAndCG {
	int size;
	int cg; //the file's cost/gain value.
	short index; //the file's location in the uniqueFiles array. Necessary for printing out cache.
};

struct requestJ {
	int s;
	int e;
	int size;
	int JobFile;
	int JobID;
	float penalty;
};

//stuff for that weird algorithm i may or may not work on...
/*
struct myAlgFile {
	int size;
	int cg;
	short index;
	int *relCosts;
};
bool operator <(const myAlgFile& x, const myAlgFile& y) {
	if (x.size == y.size) {
		return x.index < y.index;
	}
	else {
		return x.size < y.size;
	}
}
bool operator ==(const myAlgFile& x, const myAlgFile& y) {
	return x.index == y.index;
}*/

struct weightedDistFile {
	int size;
	short index;
	vector<int> occurrences;
	float tempcost;	//used to not recompute the cost/gain at a given index
};

struct hash_intV {
	size_t operator()(const vector<int> x) const {
		int r = 0;
		for (int i = 0; i < x.size(); i++){
			r = r + std::hash<int>()(x[i]);
		}
		return r;
	}
};

struct hash_intS {
	size_t operator()(const std::unordered_set<int> x) const {
		int r = 0;
		std::unordered_set<int>::iterator mneh = x.begin();
		while(mneh != x.end()) {
			r = r + *mneh;
			std::advance(mneh, 1);
		}
		return r;
	}
};
bool operator <(const fileSizeAndCG& x, const fileSizeAndCG& y) {
	return x.cg < y.cg;
}
bool operator ==(const fileSizeAndCG& x, const fileSizeAndCG& y) {
	return x.index == y.index;
}
bool operator ==(const requestJ x, const requestJ y) {
	return (x.s == y.s && x.e == y.e && x.JobID == y.JobID && x.size == y.size);
}

template <typename T> bool PComp(const T * const &a, const T * const &b)
{
	return *a < *b;
}

int totalcost = 0;					//keeps track of how much data was accessed from slow memory (i.e. not found in cache).
int totalcostbrute = 0;				//keeps track of how data was accessed from the brute force algorithm.
fileSizeAndCG *uniqueFiles;			//array of unique files; randomly generated based on user parameters or read in from text file.
int numUniqueFiles;					//number of unique files; given by user or text file.
int numFileAccesses;				//number of file accesses; given by user or text file.
int cacheSize;						//size of cache; given by user or text file. Note that currently the cache must be >= the upper bound of possible file size.
int FSizeL;							//lower bound of possible file size; given by user. Not needed for input from text file.
int FSizeU;							//upper bound of possible file size; given by user. Not needed for input from text file.
int *accessList;					//list of indexes to uniqueFiles; randomly generated based on user parameters or read in from text file.
bool WILLPRINT = true;
int printTestCase = 600;

list<fileSizeAndCG*> cacheList;		//the cache, stored as a linked list.
int usedList = 0;					//how much space on the cache is currently being used.

ifstream inFile;					//input file stream.

std::unordered_map<vector<int>, int*, hash_intV> history;
bool needHistoryFreed = false;

int weightedIndex = 0;		//used in the comparison function for the weighted algo

void createRandUniqueFiles() { //The amount of files is given by the user. The lower and upper bounds of the file sizes is given by the user.
	//uniqueFiles = (fileSizeAndCG *)malloc(sizeof(fileSizeAndCG) * numUniqueFiles);
	uniqueFiles = new fileSizeAndCG[numUniqueFiles];
	if (uniqueFiles == NULL) {
		cout << "Failed to allocate for unique files. Press any key to exit.\n";
		_getch();
		exit(EXIT_FAILURE);
	}
	if (FSizeL == FSizeU) { //There is only one possible file size. The rand function doesn't like it when fsizel == fsizeu.
		for (int i = 0; i < numUniqueFiles; i++) {
			uniqueFiles[i].size = FSizeL;
			uniqueFiles[i].cg = 0;
			uniqueFiles[i].index = i;
		}
		return;
	}
	for (int i = 0; i < numUniqueFiles; i++) {
		uniqueFiles[i].size = rand() % (FSizeU - FSizeL) + FSizeL;
		uniqueFiles[i].cg = 0;
		uniqueFiles[i].index = i;
	}
}

void createRandAccessList() { //THe amount of accesses is given by the user.
	//accessList = (int *)malloc(sizeof(int) * numFileAccesses);
	accessList = new int[numFileAccesses];
	if (accessList == NULL) {
		cout << "Failed to allocate for access list. Press any key to exit.\n";
		_getch();
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < numFileAccesses; i++) {
		accessList[i] = rand() % numUniqueFiles;
	}
	for (int i = 0; i < numFileAccesses; i++) { //Now that we have the access list, we can get each unique file's cost/gain value.
		uniqueFiles[(accessList[i])].cg += uniqueFiles[(accessList[i])].size;
	}
}

void printCache() {
	cout << "Printing out cache...\n";
	list<fileSizeAndCG*>::iterator printer = cacheList.begin();
	while (printer != cacheList.end()) {
		cout << "index: " << (**printer).index << ", cg: " << (**printer).cg << "   ";
		std::advance(printer, 1);
	}
	cout << "\n" << "Used cache space: " << usedList <<"\n";
}

void printAccessList() {
	cout << "Printing out accessList...\n";
	for (int i = 0; i < numFileAccesses; i++) {
		cout << accessList[i] << " ";
	}
	cout << "\n";
}

void printUniqueFiles() {
	cout << "Printing out unique files...\n";
	for (int i = 0; i < numUniqueFiles; i++) {
		cout << "Size: " << uniqueFiles[i].size << " | C/G: " << uniqueFiles[i].cg << "      ";
	}
}

int askForInputType() {
	char input;
	cout << "Do you want to load a specific cache problem from [f]ile, or do you want to [g]enerate one based on your parameters?" << endl;
	cin >> input;
	if (input == 'f') { return 0; }
	else if (input == 'g') { return 1; }
	else { return askForInputType(); }
 }

void getInputForRand() { //Asks the user for #unique files, #accesses, upper/lower bounds of file size, cache size
	cout << "Enter number of unique files.\n";
	cin >> numUniqueFiles;
	if (numUniqueFiles < 1)
	{
		cout << "Error: Invalid number of unique files.";
		_getch();
		exit(EXIT_FAILURE);

	}
	cout << "Enter number of file accesses:\n";
	cin >> numFileAccesses;
	if (numFileAccesses < 0)
	{
		cout << "Error: Invalid number of file accesses.";
		_getch();
		exit(EXIT_FAILURE);

	}
	cout << "Enter cache size:\n";
	cin >> cacheSize;
	if (cacheSize < 1)
	{
		cout << "Error: Invalid cache size.";
		_getch();
		exit(EXIT_FAILURE);

	}
	cout << "Enter lowest file size:\n";
	cin >> FSizeL;
	if (FSizeL < 1)
	{
		cout << "Error: Invalid file size.";
		_getch();
		exit(EXIT_FAILURE);
	}
	cout << "Enter greatest file size:\n";
	cin >> FSizeU;
	if (FSizeU < 1)
	{
		cout << "Error: Invalid file size.";
		_getch();
		exit(EXIT_FAILURE);

	}
	if (FSizeU < FSizeL)
	{
		cout << "Error: Biggest file size is smaller than smallest file size.";
		_getch();
		exit(EXIT_FAILURE);
	}
	if (FSizeU > cacheSize) {
		cout << "Error: Biggest file size is larger than available cache. To be dealt with later.";
		_getch();
		exit(EXIT_FAILURE);
	}

	createRandUniqueFiles();
	createRandAccessList();

	return;
}

void createUserUniqueFiles() {
	//uniqueFiles = (fileSizeAndCG *)malloc(sizeof(fileSizeAndCG) * numUniqueFiles);
	uniqueFiles = new fileSizeAndCG[numUniqueFiles];
	if (uniqueFiles == NULL) {
		cerr << "Failed to allocate for unique files. Press any key to exit.\n";
		_getch();
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < numUniqueFiles; i++) {
		inFile >> uniqueFiles[i].size;
		if (uniqueFiles[i].size > cacheSize || uniqueFiles[i].size < 1) {
			cerr << "Size of file exceedes cache size, or is < 1. Press any key to exit.\n";
			_getch();
			exit(1);
		}
		uniqueFiles[i].cg = 0;
		uniqueFiles[i].index = i;
	}
}

void createUserAccessList() {
	//accessList = (int *)malloc(sizeof(int) * numFileAccesses);
	accessList = new int[numFileAccesses];
	if (accessList == NULL) {
		cout << "Failed to allocate for access list. Press any key to exit.\n";
		_getch();
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < numFileAccesses; i++) {
		inFile >> accessList[i];
		if (accessList[i] > numUniqueFiles - 1 || accessList[i] < 0) {
			cerr << "Invalid file detected. Press any key to exit.\n";
			_getch();
			exit(1);
		}
	}
	for (int i = 0; i < numFileAccesses; i++) { //Now that we have the access list, we can get each unique file's cost/gain value.
		uniqueFiles[(accessList[i])].cg += uniqueFiles[(accessList[i])].size;
	}
}

void getInputFromFile() {
	cout << "\nPlease enter file directory and name.\n";
	string myfilename;
	cin >> myfilename;
	inFile.open(myfilename);
	if (!inFile) {
		cerr << "Unable to open file " << myfilename;
		_getch();
		exit(1);   // call system to stop
	}
	inFile >> numUniqueFiles;
	if (numUniqueFiles < 1)
	{
		cout << "Error: Invalid number of unique files.";
		_getch();
		exit(EXIT_FAILURE);

	}
	inFile >> numFileAccesses;
	if (numFileAccesses < 0)
	{
		cout << "Error: Invalid number of file accesses.";
		_getch();
		exit(EXIT_FAILURE);

	}
	inFile >> cacheSize;
	if (cacheSize < 1)
	{
		cout << "Error: Invalid cache size.";
		_getch();
		exit(EXIT_FAILURE);

	}
	/*inFile >> FSizeL;
	if (FSizeL < 1)
	{
		cout << "Error: Invalid file size.";
		_getch();
		exit(EXIT_FAILURE);
	}
	inFile >> FSizeU;
	if (FSizeU < 1)
	{
		cout << "Error: Invalid file size.";
		_getch();
		exit(EXIT_FAILURE);

	}
	if (FSizeU < FSizeL)
	{
		cout << "Error: Biggest file size is smaller than smallest file size.";
		_getch();
		exit(EXIT_FAILURE);
	}
	if (FSizeU > cacheSize) {
		cout << "Error: Biggest file size is larger than available cache. To be dealt with later.";
		_getch();
		exit(EXIT_FAILURE);
	}*/

	createUserUniqueFiles();
	createUserAccessList();

	inFile.close();
}

string ExePath() {
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	string::size_type pos = string(buffer).find_last_of("\\/");
	return string(buffer).substr(0, pos);
}

void sendInputToFile() { //Lets the user write the current test case to a file in case they feel like re-running it later.
	cout << "Input filename. It will be saved to " << ExePath() << "\n";
	string outfilename;
	cin >> outfilename;
	ofstream outFile(outfilename);
	outFile << numUniqueFiles << " " << numFileAccesses << " " << cacheSize << "\n";
	for (int i = 0; i < numUniqueFiles; i++) {
		outFile << uniqueFiles[i].size << " ";
	}
	outFile << "\n";
	for (int i = 0; i < numFileAccesses; i++) {
		outFile << accessList[i] << " ";
	}
}

void inputFilePrinter(ofstream& outf) {
	outf << numUniqueFiles << " " << numFileAccesses << " " << cacheSize << "\n";
	for (int i = 0; i < numUniqueFiles; i++) {
		outf << uniqueFiles[i].size << " ";
	}
	outf << "\n";
	for (int i = 0; i < numFileAccesses; i++) {
		outf << accessList[i] << " ";
	}
	outf << "\n";
}

void outputGlobalCachePrinter(ofstream& outf) {	//prints global cache stuff.
	list<fileSizeAndCG*>::iterator it = cacheList.begin();
	int csum = 0;
	while (it != cacheList.end()) {
		//csum += (*it)->size;
		csum++;
		it++;
	}
	outf << csum << " ";
	it = cacheList.begin();
	while (it != cacheList.end()) {
		outf << (*it)->index << " ";
		it++;
	}
	outf << "\n";
	return;
}

void HyCacheAlgo() {
	cacheList.clear();
	usedList = 0;
	string hyoutName = "HyCacheAlgo" + to_string(printTestCase) + ".txt";
	ofstream HyOut(hyoutName);
	if (WILLPRINT) {
		inputFilePrinter(HyOut);
	}
	for (int i = 0; i < numFileAccesses; i++) {
		cacheList.sort(PComp<fileSizeAndCG>); //I assume an increase in cost the further down the cache list you go, so I need to make sure it stays sorted.
		if (WILLPRINT) {
			outputGlobalCachePrinter(HyOut);
		}
		//cout << "\n \nLooking at access index " << i << " and unique file " << accessList[i] << ". Current cache is:\n";  //printing stuff out
		//printCache();
		uniqueFiles[accessList[i]].cg -= uniqueFiles[accessList[i]].size; //Reduce the c/g value of the file we're currently looking at in the acces list.
		list<fileSizeAndCG*>::iterator p;
		p = std::find(cacheList.begin(), cacheList.end(), &uniqueFiles[accessList[i]]);  //Checks if file i is already in the cache; It's sorta hacky
		if (p != cacheList.end()) {
			//cout << "FOUND IN CACHE.\n"; //We found it. We're done with the current file in the access list.
		}
		else {
			totalcost += uniqueFiles[accessList[i]].size; //It wasn't in the cache, so we'll definitely be accessing it from slow memory, increasing total cost.
			//cout << "NOT FOUND IN CACHE.\n";
			//Will the file fit into the cache right now?
			if (uniqueFiles[accessList[i]].size <= cacheSize - usedList) {
				//Yes, so let's add it to the cache.
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				usedList += uniqueFiles[accessList[i]].size;
			}
			else {
				//No, so let's figure out if it'll be worth adding to the cache.
				int currgain = uniqueFiles[accessList[i]].cg; //The gain from adding it to the cache.
				int currcost = 0; //The cost of removing necessary files.
				int potentialfree = cacheSize - usedList; //potentialfree needs to be >= the pending file's size before it can be added.
				//cout << "\nInitial potentialfree: " << potentialfree << ", current uniquefiles:\n";
				//printUniqueFiles();
				bool shouldDiscard = false;
				list<fileSizeAndCG*>::iterator q = cacheList.begin();
				while (q != cacheList.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).cg;
					potentialfree += (**q).size;
					//cout << "\nCurrent potentialfree: " << potentialfree;
					if (potentialfree >= uniqueFiles[accessList[i]].size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
					cacheList.erase(cacheList.begin(), q);
					cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				}
			}
		}
	}
	if (WILLPRINT) {
		HyOut << totalcost << "\n";
	}
	HyOut.close();
}

void HyCacheProxAlgo() {
	cacheList.clear();
	usedList = 0;
	//To begin, I'll be resetting the c/g values of the files. This makes earlier parts of the code redundant, but needed for other algos.
	for (int i = 0; i < numUniqueFiles; i++) {
		uniqueFiles[i].cg = 0;
	}
	//The length that we go down when considering c/g is 3*numuniquefiles
	for (int i = 0; i < numUniqueFiles * 3; i++) {
		if (i >= numFileAccesses) {
			break;
		}
		else {
			uniqueFiles[accessList[i]].cg += uniqueFiles[accessList[i]].size;
		}
	}
	string hyproxoutName = "HyCacheFixedWindowAlgo" + to_string(printTestCase) + ".txt";
	ofstream HyFixWindowOut(hyproxoutName);
	if (WILLPRINT) {
		inputFilePrinter(HyFixWindowOut);
	}
	for (int i = 0; i < numFileAccesses; i++) {
		cacheList.sort(PComp<fileSizeAndCG>); 
		if (WILLPRINT) {
			outputGlobalCachePrinter(HyFixWindowOut);
		}
		//cout << "\n \nProx is Looking at access index " << i << " and unique file " << accessList[i] << ". Current cache is:\n";
		//printCache();
		uniqueFiles[accessList[i]].cg -= uniqueFiles[accessList[i]].size;
		if (i + (numUniqueFiles * 3) < numFileAccesses) {
			uniqueFiles[accessList[i + (numUniqueFiles * 3)]].cg += uniqueFiles[accessList[i + (numUniqueFiles * 3)]].size;
		}
		list<fileSizeAndCG*>::iterator p;
		p = std::find(cacheList.begin(), cacheList.end(), &uniqueFiles[accessList[i]]);
		if (p != cacheList.end()) {
			//cout << "FOUND IN CACHE.\n"; 
		}
		else {
			totalcost += uniqueFiles[accessList[i]].size;
			//cout << "NOT FOUND IN CACHE.\n";
			//Will the file fit into the cache right now?
			if (uniqueFiles[accessList[i]].size <= cacheSize - usedList) {
				//Yes, so let's add it to the cache.
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				usedList += uniqueFiles[accessList[i]].size;
			}
			else {
				//No, so let's figure out if it'll be worth adding to the cache.
				int currgain = uniqueFiles[accessList[i]].cg; //The gain from adding it to the cache.
				int currcost = 0; //The cost of removing necessary files.
				int potentialfree = cacheSize - usedList; //potentialfree needs to be >= the pending file's size before it can be added.
														  //cout << "\nInitial potentialfree: " << potentialfree << ", current uniquefiles:\n";
														  //printUniqueFiles();
				bool shouldDiscard = false;
				list<fileSizeAndCG*>::iterator q = cacheList.begin();
				while (q != cacheList.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).cg;
					potentialfree += (**q).size;
					//cout << "\nCurrent potentialfree: " << potentialfree;
					if (potentialfree >= uniqueFiles[accessList[i]].size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
					cacheList.erase(cacheList.begin(), q);
					cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				}
			}
		}
	}
	if (WILLPRINT) {
		HyFixWindowOut << totalcost << "\n";
	}
	HyFixWindowOut.close();
}

void HyCacheVarWindow() {
	cacheList.clear();
	usedList = 0;
	int *lastocc = new int[numUniqueFiles];
	int *windowfreq = new int[numUniqueFiles];
	int *totalfreq = new int[numUniqueFiles];
	for (int i = 0; i < numUniqueFiles; i++) {
		//totalfreq[i] = uniqueFiles[i].cg / uniqueFiles[i].size;
		uniqueFiles[i].cg = 0;
		lastocc[i] = INT_FAST32_MAX;
		windowfreq[i] = 0;
		totalfreq[i] = 0;
	}
	for (int n = 0; n < numFileAccesses; n++) {
		totalfreq[accessList[n]]++;
	}
	int windowLength = 0;	//How far we're looking when computing c/g values
	//Build the initial window
	bool donezo = true;
	for (int i = 0; i < numFileAccesses; i++) {
		for (int k = 0; k < numUniqueFiles; k++) {
			if(windowfreq[k]==0 && totalfreq[k] != 0){
				donezo = false;
				break;
			}
			donezo = true;
		}
		if (donezo == true) {
			break;
		}
		windowLength++;
		windowfreq[accessList[i]]++;
		lastocc[accessList[i]] = i;
		uniqueFiles[accessList[i]].cg += uniqueFiles[accessList[i]].size;
		//donezo = true;
	}
	//printUniqueFiles();
	//cout << "\n";
	string hyvaroutName = "HyCacheVariableWindowAlgo" + to_string(printTestCase) + ".txt";
	ofstream HyVarWindowOut(hyvaroutName);
	if (WILLPRINT) {
		inputFilePrinter(HyVarWindowOut);
	}
	for (int i = 0; i < numFileAccesses; i++) {
		cacheList.sort(PComp<fileSizeAndCG>);
		if (WILLPRINT) {
			outputGlobalCachePrinter(HyVarWindowOut);
		}
		//cout << "\nVARLooking at access index " << i << " and unique file " << accessList[i] << ". Current cache is:\n";  //printing stuff out
		//printCache();
		//cout << "\n" << windowLength;

		//Before we even make a choice, we gotta do some setup...
		uniqueFiles[accessList[i]].cg -= uniqueFiles[accessList[i]].size;
		windowfreq[accessList[i]]--;
		windowLength--;
		totalfreq[accessList[i]]--;
		donezo = true;
		for (int j = i + 1 + windowLength; j < numFileAccesses; j++) {
			for (int k = 0; k < numUniqueFiles; k++) {
				if (windowfreq[k] == 0 && totalfreq[k] != 0) {
					donezo = false;
					break;
				}
				donezo = true;
			}
			if (donezo == true) {
				break;
			}
			windowLength++;
			windowfreq[accessList[j]]++;
			lastocc[accessList[j]] = j;
			uniqueFiles[accessList[j]].cg += uniqueFiles[accessList[j]].size;
			//donezo = true;
		}
		
		list<fileSizeAndCG*>::iterator p;
		p = std::find(cacheList.begin(), cacheList.end(), &uniqueFiles[accessList[i]]);
		if (p != cacheList.end()) {
			//cout << "FOUND IN CACHE.\n"; 
		}
		else {
			totalcost += uniqueFiles[accessList[i]].size;
			//cout << "NOT FOUND IN CACHE.\n";
			//Will the file fit into the cache right now?
			if (uniqueFiles[accessList[i]].size <= cacheSize - usedList) {
				//Yes, so let's add it to the cache.
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				usedList += uniqueFiles[accessList[i]].size;
			}
			else {
				//No, so let's figure out if it'll be worth adding to the cache.
				int currgain = uniqueFiles[accessList[i]].cg; //The gain from adding it to the cache.
				int currcost = 0; //The cost of removing necessary files.
				int potentialfree = cacheSize - usedList; //potentialfree needs to be >= the pending file's size before it can be added.
				//cout << "\nInitial potentialfree: " << potentialfree << ", current uniquefiles:\n";
				//printUniqueFiles();
				bool shouldDiscard = false;
				list<fileSizeAndCG*>::iterator q = cacheList.begin();
				while (q != cacheList.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).cg;
					potentialfree += (**q).size;
					//cout << "\nCurrent potentialfree: " << potentialfree;
					if (potentialfree >= uniqueFiles[accessList[i]].size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
					cacheList.erase(cacheList.begin(), q);
					cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				}
			}
		}
	}
	if (WILLPRINT) {
		HyVarWindowOut << totalcost << "\n";
	}
	HyVarWindowOut.close();
}

void HyCacheVarWindowMaxFile() {	//as opposed to sum of not present files
	cacheList.clear();
	usedList = 0;
	int currcachecg = 0;
	int *windowfreq = new int[numUniqueFiles];
	int *totalfreq = new int[numUniqueFiles];
	for (int i = 0; i < numUniqueFiles; i++) {
		//totalfreq[i] = uniqueFiles[i].cg / uniqueFiles[i].size;
		uniqueFiles[i].cg = 0;
		windowfreq[i] = 0;
		totalfreq[i] = 0;
	}
	for (int n = 0; n < numFileAccesses; n++) {
		totalfreq[accessList[n]]++;
	}
	int windowLength = 0;	//How far we're looking when computing c/g values
							//Build the initial window
	int l = 0;
	for (int i = 0; i < numUniqueFiles; i++) {	//Get largest file that appears at least once
		if (uniqueFiles[i].size > l) {
			l = uniqueFiles[i].size;
		}
	}
	//bool donezo = true;
	for (int i = 0; i < numFileAccesses; i++) {
		if (currcachecg >= l) {
			break;
		}
		windowLength++;
		windowfreq[accessList[i]]++;
		uniqueFiles[accessList[i]].cg += uniqueFiles[accessList[i]].size;
		currcachecg += uniqueFiles[accessList[i]].size;
	}
	//printUniqueFiles();
	cout << "\n";
	for (int i = 0; i < numFileAccesses; i++) {
		cacheList.sort(PComp<fileSizeAndCG>);
		//cout << "\nVARLooking at access index " << i << " and unique file " << accessList[i] << ". Current cache is:\n";  //printing stuff out
		//printCache();
		//cout << "\n" << windowLength;

		//Before we even make a choice, we gotta do some setup...
		uniqueFiles[accessList[i]].cg -= uniqueFiles[accessList[i]].size;
		windowfreq[accessList[i]]--;
		windowLength--;
		totalfreq[accessList[i]]--;
		currcachecg -= uniqueFiles[accessList[i]].size;
		if (totalfreq[accessList[i]] == 0) {	//check if there's a new, lower highest file size
			l = 0;
			for (int k = 0; k < numUniqueFiles; k++) {
				if (uniqueFiles[k].size > l && totalfreq[k] != 0) {
					l = uniqueFiles[k].size;
				}
			}
		}
		for (int j = i + 1 + windowLength; j < numFileAccesses; j++) {
			if (currcachecg >= l) {
				break;
			}
			windowLength++;
			windowfreq[accessList[j]]++;
			uniqueFiles[accessList[j]].cg += uniqueFiles[accessList[j]].size;
			currcachecg += uniqueFiles[accessList[j]].size;
		}

		list<fileSizeAndCG*>::iterator p;
		p = std::find(cacheList.begin(), cacheList.end(), &uniqueFiles[accessList[i]]);
		if (p != cacheList.end()) {
			//cout << "FOUND IN CACHE.\n"; 
		}
		else {
			totalcost += uniqueFiles[accessList[i]].size;
			//cout << "NOT FOUND IN CACHE.\n";
			//Will the file fit into the cache right now?
			if (uniqueFiles[accessList[i]].size <= cacheSize - usedList) {
				//Yes, so let's add it to the cache.
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				usedList += uniqueFiles[accessList[i]].size;
			}
			else {
				//No, so let's figure out if it'll be worth adding to the cache.
				int currgain = uniqueFiles[accessList[i]].cg; //The gain from adding it to the cache.
				int currcost = 0; //The cost of removing necessary files.
				int potentialfree = cacheSize - usedList; //potentialfree needs to be >= the pending file's size before it can be added.
														  //cout << "\nInitial potentialfree: " << potentialfree << ", current uniquefiles:\n";
														  //printUniqueFiles();
				bool shouldDiscard = false;
				list<fileSizeAndCG*>::iterator q = cacheList.begin();
				while (q != cacheList.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).cg;
					potentialfree += (**q).size;
					//cout << "\nCurrent potentialfree: " << potentialfree;
					if (potentialfree >= uniqueFiles[accessList[i]].size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
					cacheList.erase(cacheList.begin(), q);
					cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				}
			}
		}
	}
}

void HyCacheVarWindowSumFiles() {	//as opposed to sum of not present files
	cacheList.clear();
	usedList = 0;
	int currwincg = 0;
	int sumAbsent = 0;
	int *windowfreq = new int[numUniqueFiles];
	int *totalfreq = new int[numUniqueFiles];
	for (int i = 0; i < numUniqueFiles; i++) {
		uniqueFiles[i].cg = 0;
		windowfreq[i] = 0;
		totalfreq[i] = 0;
		sumAbsent += uniqueFiles[i].size;
	}
	for (int n = 0; n < numFileAccesses; n++) {
		totalfreq[accessList[n]]++;
	}
	for (int i = 0; i < numUniqueFiles; i++) {
		if (totalfreq[i] == 0) {
			sumAbsent -= uniqueFiles[i].size;
		}
	}
	int windowLength = 0;	//How far we're looking when computing c/g values
							//Build the initial window
	//bool donezo = true;
	for (int i = 0; i < numFileAccesses; i++) {
		if (currwincg >= sumAbsent) {
			break;
		}
		windowLength++;
		windowfreq[accessList[i]]++;
		uniqueFiles[accessList[i]].cg += uniqueFiles[accessList[i]].size;
		currwincg += uniqueFiles[accessList[i]].size;
		if (windowfreq[accessList[i]] == 1) {
			sumAbsent -= uniqueFiles[accessList[i]].size;
		}
	}
	//printUniqueFiles();
	cout << "\n";
	for (int i = 0; i < numFileAccesses; i++) {
		cacheList.sort(PComp<fileSizeAndCG>);
		//cout << "\nVARLooking at access index " << i << " and unique file " << accessList[i] << ". Current cache is:\n";  //printing stuff out
		//printCache();
		cout << " " << windowLength;
		//Before we even make a choice, we gotta do some setup...
		uniqueFiles[accessList[i]].cg -= uniqueFiles[accessList[i]].size;
		windowfreq[accessList[i]]--;
		windowLength--;
		totalfreq[accessList[i]]--;
		currwincg -= uniqueFiles[accessList[i]].size;
		if (windowfreq[accessList[i]] == 0 && totalfreq[accessList[i]] != 0) {
			sumAbsent += uniqueFiles[accessList[i]].size;
		}
		for (int i = 0; i < numFileAccesses; i++) {
			if (currwincg >= sumAbsent) {
				break;
			}
			windowLength++;
			windowfreq[accessList[i]]++;
			uniqueFiles[accessList[i]].cg += uniqueFiles[accessList[i]].size;
			currwincg += uniqueFiles[accessList[i]].size;
			if (windowfreq[accessList[i]] == 1) {
				sumAbsent -= uniqueFiles[accessList[i]].size;
			}
		}

		list<fileSizeAndCG*>::iterator p;
		p = std::find(cacheList.begin(), cacheList.end(), &uniqueFiles[accessList[i]]);
		if (p != cacheList.end()) {
			//cout << "FOUND IN CACHE.\n"; 
		}
		else {
			totalcost += uniqueFiles[accessList[i]].size;
			//cout << "NOT FOUND IN CACHE.\n";
			//Will the file fit into the cache right now?
			if (uniqueFiles[accessList[i]].size <= cacheSize - usedList) {
				//Yes, so let's add it to the cache.
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				usedList += uniqueFiles[accessList[i]].size;
			}
			else {
				//No, so let's figure out if it'll be worth adding to the cache.
				int currgain = uniqueFiles[accessList[i]].cg; //The gain from adding it to the cache.
				int currcost = 0; //The cost of removing necessary files.
				int potentialfree = cacheSize - usedList; //potentialfree needs to be >= the pending file's size before it can be added.
														  //cout << "\nInitial potentialfree: " << potentialfree << ", current uniquefiles:\n";
														  //printUniqueFiles();
				bool shouldDiscard = false;
				list<fileSizeAndCG*>::iterator q = cacheList.begin();
				while (q != cacheList.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).cg;
					potentialfree += (**q).size;
					//cout << "\nCurrent potentialfree: " << potentialfree;
					if (potentialfree >= uniqueFiles[accessList[i]].size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
					cacheList.erase(cacheList.begin(), q);
					cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				}
			}
		}
	}
}

void HyCacheVarRelevantWindow() {
	cacheList.clear();
	usedList = 0;
	int *nextocc = new int[numUniqueFiles];
	int *windowfreq = new int[numUniqueFiles];
	int *totalfreq = new int[numUniqueFiles];
	int *adjsize = new int[numUniqueFiles];
	double smallest = DBL_MAX;
	for (int i = 0; i < numUniqueFiles; i++) {
		//totalfreq[i] = uniqueFiles[i].cg / uniqueFiles[i].size;
		uniqueFiles[i].cg = 0;
		nextocc[i] = INT_MAX;
		windowfreq[i] = 0;
		totalfreq[i] = 0;
		if (uniqueFiles[i].size < smallest) {
			smallest = (double) uniqueFiles[i].size;
		}
	}
	for (int n = 0; n < numFileAccesses; n++) {
		totalfreq[accessList[n]]++;
		if (nextocc[accessList[n]] == INT_MAX) {
			nextocc[accessList[n]] = n;
		}
	}
	int windowLength = 0;	//How far we're looking when computing c/g values
							//Build the initial window
	int windowLimit = 0;
	for (int i = 0; i < numUniqueFiles; i++) {
		adjsize[i] = (int)ceil((uniqueFiles[i].size / smallest));
		if (adjsize[i] >= windowLimit && adjsize[i] >= nextocc[i]) {
			windowLimit = adjsize[i];
		}
	}
	for (int i = 0; i < numFileAccesses; i++) {
		for (int k = 0; k < numUniqueFiles; k++) {	//Find the nearest relevant big file
			//AT LEAST ONE OCCURRENCE OTHW
		}
		windowLength++;
		windowfreq[accessList[i]]++;
		nextocc[accessList[i]] = i;
		uniqueFiles[accessList[i]].cg += uniqueFiles[accessList[i]].size;
	}
	//printUniqueFiles();
	//cout << "\n";
	for (int i = 0; i < numFileAccesses; i++) {
		cacheList.sort(PComp<fileSizeAndCG>);
		//cout << "\nVARLooking at access index " << i << " and unique file " << accessList[i] << ". Current cache is:\n";  //printing stuff out
		//printCache();
		//cout << "\n" << windowLength;

		//Before we even make a choice, we gotta do some setup...
		uniqueFiles[accessList[i]].cg -= uniqueFiles[accessList[i]].size;
		windowfreq[accessList[i]]--;
		windowLength--;
		totalfreq[accessList[i]]--;
		for (int j = i + 1 + windowLength; j < numFileAccesses; j++) {
			for (int k = 0; k < numUniqueFiles; k++) {
			}
			windowLength++;
			windowfreq[accessList[j]]++;
			nextocc[accessList[j]] = j;
			uniqueFiles[accessList[j]].cg += uniqueFiles[accessList[j]].size;
		}

		list<fileSizeAndCG*>::iterator p;
		p = std::find(cacheList.begin(), cacheList.end(), &uniqueFiles[accessList[i]]);
		if (p != cacheList.end()) {
			//cout << "FOUND IN CACHE.\n"; 
		}
		else {
			totalcost += uniqueFiles[accessList[i]].size;
			//cout << "NOT FOUND IN CACHE.\n";
			//Will the file fit into the cache right now?
			if (uniqueFiles[accessList[i]].size <= cacheSize - usedList) {
				//Yes, so let's add it to the cache.
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				usedList += uniqueFiles[accessList[i]].size;
			}
			else {
				//No, so let's figure out if it'll be worth adding to the cache.
				int currgain = uniqueFiles[accessList[i]].cg; //The gain from adding it to the cache.
				int currcost = 0; //The cost of removing necessary files.
				int potentialfree = cacheSize - usedList; //potentialfree needs to be >= the pending file's size before it can be added.
														  //cout << "\nInitial potentialfree: " << potentialfree << ", current uniquefiles:\n";
														  //printUniqueFiles();
				bool shouldDiscard = false;
				list<fileSizeAndCG*>::iterator q = cacheList.begin();
				while (q != cacheList.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).cg;
					potentialfree += (**q).size;
					//cout << "\nCurrent potentialfree: " << potentialfree;
					if (potentialfree >= uniqueFiles[accessList[i]].size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
					cacheList.erase(cacheList.begin(), q);
					cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
				}
			}
		}
	}
}

vector< vector<int> > getAllSubsets(vector<int> set)
{
	vector< vector<int> > subset;
	vector<int> empty;
	subset.push_back(empty);

	for (int i = 0; i < set.size(); i++)
	{
		vector< vector<int> > subsetTemp = subset;

		for (int j = 0; j < subsetTemp.size(); j++)
			subsetTemp[j].push_back(set[i]);

		for (int j = 0; j < subsetTemp.size(); j++)
			subset.push_back(subsetTemp[j]);
	}
	return subset;
}

void showPossibleCaches(vector< vector<int> > possibleCaches) {
	vector< vector<int> >::iterator p = possibleCaches.begin();
	while (p != possibleCaches.end()) {
		vector<int>::iterator q = p->begin();
		while (q != p->end()) {
			cout << uniqueFiles[*q].index << " ";
			std::advance(q, 1);
		}
		cout << "\n";
		std::advance(p, 1);
	}
	cout << "\n \n";
	return;
}

void showSinglePossibleCache(vector<int> pCache) {
	vector<int>::iterator q = pCache.begin();
	while (q != pCache.end()) {
		cout << uniqueFiles[*q].index << " ";
		std::advance(q, 1);
	}
	cout << "\n";
	return;
}

int bruteForceHelper(vector<int> bruteCache, int alindex) {
	if (alindex == numFileAccesses) {
		//cout << "Got to the end of the accesslist just now... " << encCount << "\n";
		//encCount++;
		return 0;
	}

	if (bruteCache.empty() == false) {
		if (std::find(bruteCache.begin(), bruteCache.end(), accessList[alindex]) != bruteCache.end()) {	//file found in cache
			return bruteForceHelper(bruteCache, alindex + 1);
		}
	}
	else {
		vector<int> temp;
		temp.push_back(accessList[alindex]);
		int ecMincost = uniqueFiles[accessList[alindex]].size + min(bruteForceHelper(bruteCache, alindex + 1), bruteForceHelper(temp, alindex + 1));
		return ecMincost;
	}

	vector<int> possibleCosts;
	possibleCosts.push_back(100000);
	
	vector< vector<int> > possibleCaches = getAllSubsets(bruteCache);
	//showPossibleCaches(possibleCaches);
	vector< vector<int> >::iterator p = possibleCaches.begin();
	//vector< vector<int> >::iterator cacheToPrint = p;
	//int TEMPmincost = 100000;
	while (p != possibleCaches.end()) {
		int tempsize = 0;
		vector<int>::iterator q = p->begin();
		//int val = 0;
		while (q != p->end()) {	//adding up sizes
			tempsize += uniqueFiles[*q].size;
			//cout << uniqueFiles[*q].index << " ";
			std::advance(q, 1);
		}
		//cout << "\n";

		/*
			First, we see how things are if we don't add the current file to the cache...
		*/
		int tempvalnop = uniqueFiles[accessList[alindex]].size + bruteForceHelper(*p, alindex + 1);
		possibleCosts.push_back(tempvalnop);
		//if (tempvalnop < TEMPmincost) {
		//	cacheToPrint = p;
		//	TEMPmincost = tempvalnop;
		//}
		if (cacheSize - tempsize >= uniqueFiles[accessList[alindex]].size) {
			//add currfile to p
			p->insert(p->begin(), accessList[alindex]);
			int tempval = uniqueFiles[accessList[alindex]].size + bruteForceHelper(*p, alindex + 1);
			//if (tempval < TEMPmincost) {
			//	TEMPmincost = tempval;
				//possibleCosts.push_back(tempval);
			//	cacheToPrint = p;
			//}
			//possibleCosts.push_back(uniqueFiles[accessList[alindex]].size + bruteForceHelper(*p, alindex + 1, false)); //recursive call
			possibleCosts.push_back(tempval);
		}
		std::advance(p, 1);
	}
	//cout << "\n";
	//sort possiblecosts; return minimum
	
	int mincost = 100000;
	vector<int>::iterator m = possibleCosts.begin();
	while (m != possibleCosts.end()) {
		if (*m < mincost) {
			mincost = *m;
		}
		std::advance(m, 1);
	}
	
	//showSinglePossibleCache(*smallest);
	return mincost;
}

void bruteForceAlgo() {
	vector<int> bruteCache;
	totalcostbrute = bruteForceHelper(bruteCache, 0);
}

void printIntVector(vector <int> intv) {
	for (int i = 0; i < intv.size(); i++) {
		cout << intv[i] << " ";
	}
	cout << "\n";
}

void buildHistoryMemo() {
	//cout << "\nRan buildHistoryMemo... Displaying possible caches.\n";
	vector<int> allFilesCache;
	for (int i = 0; i < numUniqueFiles; i++) {
		allFilesCache.push_back(uniqueFiles[i].index);
	}
	vector<vector<int>> allPossibleCachesEver = getAllSubsets(allFilesCache);
	vector<vector<int>>::iterator p = allPossibleCachesEver.begin();
	while (p != allPossibleCachesEver.end()) {
		//cout << "\nVector: ";
		//printIntVector(*p);
		//hash_intV nullguy;
		//cout << "\nHash value: " << nullguy.operator()(*p);
		int tempsum = 0;
		vector<int>::iterator q = p->begin();
		while (q != p->end()) {
			tempsum += uniqueFiles[*q].size;
			std::advance(q, 1);
		}
		if (tempsum <= cacheSize) {
			//history[*p] = (int*)malloc(sizeof(int) * numFileAccesses);
			history[*p] = new int[numFileAccesses];
			if (history[*p] == NULL) {
				cerr << "Failed to allocate stuff for memo history. Press any key to quit.";
				_getch();
				exit(EXIT_FAILURE);
			}
			for (int j = 0; j < numFileAccesses; j++) {	//Initialize history to -1
				history[*p][j] = -1;
			}
		}
		else {
			history[*p] = NULL;
		}
		std::advance(p, 1);
	}
	needHistoryFreed = true;
	//cout << "\n\n";
}
/*
void buildHistoryMemoAlt() {
	vector<int> allFilesCache;
	for (int i = 0; i < numUniqueFiles; i++) {
		allFilesCache.push_back(uniqueFiles[i].index);
	}
	vector<vector<int>> allPossibleCachesEver = getAllSubsets(allFilesCache);
	vector<vector<int>>::iterator p = allPossibleCachesEver.begin();
	while (p != allPossibleCachesEver.end()) {
		//cout << "\nVector: ";
		//printIntVector(*p);
		//hash_intV nullguy;
		//cout << "\nHash value: " << nullguy.operator()(*p);
		int tempsum = 0;
		vector<int>::iterator q = p->begin();
		while (q != p->end()) {
			tempsum += uniqueFiles[*q].size;
			std::advance(q, 1);
		}
		if (tempsum <= cacheSize) {
			history[*p] = (int*)malloc(sizeof(int) * numFileAccesses);
			if (history[*p] == NULL) {
				cerr << "Failed to allocate stuff for memo history. Press any key to quit.";
				_getch();
				exit(EXIT_FAILURE);
			}
			/*for (int j = 0; j < numFileAccesses; j++) {	//Initialize history to -1
				history[*p][j] = -1;
			}
			if (std::find((*p).begin, (*p).end, accessList[numFileAccesses - 1]) != (*p).end) {
				history[*p][numFileAccesses - 1] = 0;
			}
			else {
				history[*p][numFileAccesses - 1] = uniqueFiles[accessList[numFileAccesses - 1]].size;
			}
		}
		else {
			history[*p] = NULL;
		}
		std::advance(p, 1);
	}

	for (int k = 1; k < numFileAccesses; k++) {

	}
	needHistoryFreed = true;
	//cout << "\n\n";
}*/

/*
	OK, so there are a few things I think that I can do to optimize brute memo...
	First of all, I need to think of a way to get relevant subcaches. That is, the full caches that contain a mixture of their current files,
	and the current file being accessed. I guess the best way to do it still would be to generate
	all possible subcaches of that list files, and ignore ones that are too large or too small
	according to what I've written on that one paper.
*/

int bruteForceMemoHelper(vector<int> bruteCache, int alindex) {
	//cout << "is hsi runnign?\n";
	std::sort(bruteCache.begin(), bruteCache.end());
	if (alindex == numFileAccesses) {
		return 0;
	}
	/*
		Bit that checks if a cost was already computed for a given cache/index:
	*/
	int *ptr = history[bruteCache];
	//cout << "\n" << ptr << "\n";
	/*int PREVTHING = ptr[alindex];
	if (PREVTHING != -1) {
		return PREVTHING;
	}*/

	//cout << "Current brutecache in memo:\n";
	//printIntVector(bruteCache);
	if (ptr == NULL) {
		cout << "\n\n\n";
		printIntVector(bruteCache);
		hash_intV nullguy;
		cout << "Bad Vector gave hash value: " << nullguy.operator()(bruteCache) << " \n";
		_getch();
		exit(EXIT_FAILURE);
	}
	int z = ptr[alindex];
	if (z != -1) {
		//cout << "\nfound in history; currently lookin at " << alindex;
		return z;
	}

	if (bruteCache.empty() == false) {
		if (std::find(bruteCache.begin(), bruteCache.end(), accessList[alindex]) != bruteCache.end()) {	//file found in cache
			int tempz = bruteForceMemoHelper(bruteCache, alindex + 1);
			ptr[alindex] = tempz;
			return tempz;
		}
	}
	else {
		/*vector<int> temp;
		temp.push_back(accessList[alindex]);
		int ecMincost = uniqueFiles[accessList[alindex]].size + min(bruteForceMemoHelper(bruteCache, alindex + 1), bruteForceMemoHelper(temp, alindex + 1));
		ptr[alindex] = ecMincost;
		return ecMincost;*/
		//Original version recursively checked both with and without adding the current file, but not adding it is pointless.
		bruteCache.push_back(accessList[alindex]);
		int m = uniqueFiles[accessList[alindex]].size + bruteForceMemoHelper(bruteCache, alindex + 1);
		ptr[alindex] = m;
		return m;
	}

	vector<int> possibleCosts;
	//possibleCosts.push_back(100000);
	//bruteCache.push_back(accessList[alindex]);
	vector< vector<int> > possibleCaches = getAllSubsets(bruteCache);
	vector< vector<int> >::iterator p = possibleCaches.begin();
	while (p != possibleCaches.end()) {
		int tempsize = 0;
		vector<int>::iterator q = p->begin();
		while (q != p->end()) {	//adding up sizes
			tempsize += uniqueFiles[*q].size;
			std::advance(q, 1);
		}
		int tempvalnop = uniqueFiles[accessList[alindex]].size + bruteForceMemoHelper(*p, alindex + 1);
		possibleCosts.push_back(tempvalnop);
		if (cacheSize - tempsize >= uniqueFiles[accessList[alindex]].size) {
		//if(cacheSize >= tempsize){
			p->insert(p->begin(), accessList[alindex]);
			int tempval = uniqueFiles[accessList[alindex]].size + bruteForceMemoHelper(*p, alindex + 1);
			possibleCosts.push_back(tempval);
		}
		std::advance(p, 1);
	}

	int mincost = 1000000;
	vector<int>::iterator m = possibleCosts.begin();
	while (m != possibleCosts.end()) {
		if (*m < mincost) {
			mincost = *m;
		}
		std::advance(m, 1);
	}
	ptr[alindex] = mincost;
	//cout << "Finished a biggo at " << alindex;
	return mincost;
}

void bruteForceMemoAlgo() {
	vector<int> bruteCache;
	totalcostbrute = bruteForceMemoHelper(bruteCache, 0);
}

int askForAlgoType() {
	char input;
	char memoYes = '^';
	cout << "Do you want to run just the [h]ycache algorithm, the [e]xhaustive brute force algorithm, or [b]oth and compare? Hycache is default." << endl;
	cin >> input;
	if (input == 'e' || input == 'b') {
		cout << "Do you want to use the [m]emoized brute force algorithm, [n]ot, or [b]oth? Memoized is default.";
		cin >> memoYes;
	}
	if (input == 'h'){
		return 0;
	}
	else if (input == 'e') {
		if (memoYes == 'n') {
			return 1;
		}
		else if (memoYes == 'b') {
			return 2;
		}
		else {
			return 3;
		}
	}
	else if (input == 'b'){
		if (memoYes == 'n') {
			return 4;
		}
		else if (memoYes == 'b') {
			return 5;
		}
		else {
			return 6;
		}
	}
	else {
		return askForAlgoType();
	}
}

void freeHistory() {
	vector<int> allFilesCache;
	for (int i = 0; i < numUniqueFiles; i++) {
		allFilesCache.push_back(uniqueFiles[i].index);
	}
	vector<vector<int>> allPossibleCachesEver = getAllSubsets(allFilesCache);
	vector<vector<int>>::iterator p = allPossibleCachesEver.begin();
	while (p != allPossibleCachesEver.end()) {
		//free(history[*p]);
		delete history[*p];
		std::advance(p, 1);
	}
	allPossibleCachesEver.clear();
}

/*
int myWeirdAlgo() {
	//Initialize file stuffs...
	myAlgFile *algfiles = (myAlgFile*)malloc(sizeof(myAlgFile)*numUniqueFiles);
	for (int i = 0; i < numUniqueFiles; i++) {
		algfiles[i].relCosts = (int*)malloc(sizeof(int)*numUniqueFiles);
		algfiles[i].index = i;
		algfiles[i].size = uniqueFiles[i].size;
		algfiles[i].cg = 0;
	}
	//Gotta iterate through the accessList...
	int weirdcost = 0;
	vector<myAlgFile*> weirdCache;
	int usedWeirdCache = 0;
	for (int i = numFileAccesses - 1; i < numFileAccesses; i++) {
		for (int j = 0; j < numUniqueFiles; j++) {
			algfiles[j].relCosts[accessList[i]] += algfiles[accessList[i]].size;	//increment the number of times file i appears in the access list before file j appears.
		}
		for (int j = 0; j < numUniqueFiles; j++) {
			algfiles[accessList[i]].relCosts[j] = 0;
		}
		if (std::find(weirdCache.begin(), weirdCache.end(), accessList[i]) != weirdCache.end()) { //found it in cache
			continue;
		}
		else { 		//Didn't find it; we have to gather things that will potentially be replaced by the current file we're looking at.
			int potentialweirdfree = cacheSize - usedWeirdCache;
			bool shouldDiscard = false;
			vector<myAlgFile*>::iterator q = weirdCache.begin();
			while (q != weirdCache.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
				currcost += (**q).cg;
				potentialfree += (**q).size;
				//cout << "\nCurrent potentialfree: " << potentialfree;
				if (potentialfree >= uniqueFiles[accessList[i]].size) {
					if (currgain > currcost) {
						shouldDiscard = true;
					}
					else {
						//do nothing; nothing will be discarded; pending file won't be added.
					}
					std::advance(q, 1);
					break;
				}
				std::advance(q, 1);
			}
			if (shouldDiscard) {
				usedList = cacheSize + uniqueFiles[accessList[i]].size - potentialfree;
				cacheList.erase(cacheList.begin(), q);
				cacheList.insert(cacheList.begin(), &uniqueFiles[accessList[i]]);
			}
		}
	}
}*/

bool compWCache(weightedDistFile* x, weightedDistFile* y) {
	//if (y->occurrences.size() == 0) {
	//	return false;
	//}
	//else if (x->occurrences.size() == 0) {
	//	return true;
	//}
	//else {
		float xsum = 0;
		float ysum = 0;
		for (int i = 0; i < x->occurrences.size(); i++) {
			xsum += (x->size) / (x->occurrences[i] - weightedIndex);
		}
		for (int j = 0; j < y->occurrences.size(); j++) {
			ysum += (y->size) / (y->occurrences[j] - weightedIndex);
		}
		x->tempcost = xsum;
		y->tempcost = ysum;
		return xsum < ysum;
	//}
}

void outputPrintWdCache(vector<weightedDistFile*>& wCache, ofstream& outf) {
	vector<weightedDistFile*>::iterator it = wCache.begin();
	int csum = 0;
	while (it != wCache.end()) {
		//csum += (*it)->size;
		csum++;
		it++;
	}
	outf << csum << " ";
	it = wCache.begin();
	while (it != wCache.end()) {
		outf << (*it)->index << " ";
		it++;
	}
	outf << "\n";
	return;
}

int weightedDistAlgo() {
	//Get all occurrences of each file...
	//weightedDistFile *wfiles = (weightedDistFile*)malloc(sizeof(weightedDistFile)*numUniqueFiles);
	weightedDistFile *wfiles = new weightedDistFile[numUniqueFiles];
	if (wfiles == NULL) {
		cerr << "Failed to allocate space for wfiles.";
		_getch();
		exit(EXIT_FAILURE);
	}
	for (int j = 0; j < numUniqueFiles; j++) {
		wfiles[j].size = uniqueFiles[j].size;
		wfiles[j].index = j;
	}
	for (int i = numFileAccesses - 1; i >= 0; i--) {
		wfiles[accessList[i]].occurrences.push_back(i);
	}
	
	vector<weightedDistFile*> wCache;
	int usedwCache = 0;
	int wcost = 0;
	string wdoutName = "WeightedDistanceAlgo" + to_string(printTestCase) + ".txt";
	ofstream WdOut(wdoutName);
	if (WILLPRINT) {
		inputFilePrinter(WdOut);
	}
	for (int i = 0; i < numFileAccesses; i++) {
		if (WILLPRINT) {	//Note: Cache not guaranteed to be sorted here
			outputPrintWdCache(wCache, WdOut);
		}
		//cout << "\n\nGoing through iteration " << i;
		weightedDistFile* currF = &wfiles[accessList[i]];
		if (currF->occurrences.empty() == false) {
			if (currF->occurrences.back() == i) {
				currF->occurrences.pop_back(); //we've passed a given occurrence; get it off the list.
				//cout << "\nGot rid of current occurrence...";
			}
		}
		vector<weightedDistFile*>::iterator p;
		p = std::find(wCache.begin(), wCache.end(), currF);  //Checks if file i is already in the cache; It's sorta hacky
		if (p != wCache.end()) {	//found in cache
			//continue?
			//cout << "\nFound in cache...";
		}
		else {	//not found in cache
			int temps = currF->size;
			wcost += temps;
			//Will it fit in the cache? If so, just stick it in there.
			if (cacheSize - usedwCache >= temps) {
				wCache.push_back(currF);
				usedwCache += temps;
				//cout << "\n Not found in cache, but it fits, so we'll just stick it in there.";
				//contiue?
			}
			else {	//how worth it is it to add the current fiel to the cache?
				weightedIndex = i;
				std::sort(wCache.begin(), wCache.end(), &compWCache);
				//cout << "\nWD cache b4:, at index " << i << "\n";
				//vector<weightedDistFile*>::iterator zed = wCache.begin();
				//while (zed != wCache.end()) {
				//	cout << "File " << (*zed)->index << ", tempcost: " << (*zed)->tempcost << ", occurences: " << (*zed)->occurrences.size() << ". \n";
				//	zed++;
				//}
				int potentialfree = cacheSize - usedwCache; //potentialfree needs to be >= the pending file's size before it can be added.
				bool shouldDiscard = false;
				float currcost = 0;
				float currgain = 0;
				currF->tempcost = 0;
				for (int a = 0; a < currF->occurrences.size(); a++) {	//current gain
					currF->tempcost += currF->size / (currF->occurrences[a] - i);
				}
				currgain = currF->tempcost;
				//cout << "\ncurrcost of file " << currF->index << " is " << currcost;
				vector<weightedDistFile*>::iterator q = wCache.begin();
				while (q != wCache.end()) { //iterate through the list until potentialfree >= pending file's size. If currgain > currcost, add the pending file and discard the other files. Otherwise, don't add the pending file.
					currcost += (**q).tempcost;
					potentialfree += (**q).size;
					if (potentialfree >= currF->size) {
						if (currgain > currcost) {
							shouldDiscard = true;
						}
						else {
							//do nothing; nothing will be discarded; pending file won't be added.
						}
						std::advance(q, 1);
						break;
					}
					std::advance(q, 1);
				}
				if (shouldDiscard) {
					//cout << "\nNot found in cache, won't fit, decided to evict stuff.";
					usedwCache = cacheSize + currF->size - potentialfree;
					wCache.erase(wCache.begin(), q);
					wCache.push_back(currF);
					//cout << "\nNew Cache: ";
				}
				else {
					//"\nNot found in cache, won't fit, not evicting.";
				}
			}
		}
	}
	if (WILLPRINT) {
		WdOut << wcost << "\n";
	}
	WdOut.close();
	return wcost;
}

/*void barNoy() {
	int* widths;
	widths = new int[numFileAccesses];	//Each element of widths will have the total width of all jobs that occur in that time, NOT just how much overflow we have out of the cache.
	//First, create the requestJs and see how much bandwidth is being used at each time step.
	vector<requestJ> requestBlocks;
	vector<requestJ> removedBlocks;
	for (int i = 0; i < numFileAccesses; i++) {
		int tempID = accessList[i];
		int k = i+1;
		while (k != numFileAccesses) {
			if (accessList[k] == tempID) {
				break;
			}
			k++;
		}
		if (k != numFileAccesses) {
			requestJ *rj = new requestJ;
			rj->s = i;
			rj->e = k-1;
			rj->jID = accessList[i];
			rj->size = uniqueFiles[accessList[i]].size;
			requestBlocks.push_back(*rj);
		}
	}
	for (int j = 0; j < requestBlocks.size(); j++) {
		for (int k = requestBlocks[j].s; k <= requestBlocks[j].e; k++) {
			widths[k] += requestBlocks[j].size;
		}
	}
	//Now, we need to find where the largest element of widths is
	int dstar = (std::max_element(widths, widths + numFileAccesses) - widths);	//Note that dstar is an INDEX.
	//Now, we do the p2 thing, where P = rp1 + p2. Here, we consider the jobs that occur during dstar. The penalty function is just each job's size.
	//First, let's get a list of the jobs that occur at dstar.
	vector<requestJ> rbAtDstar;
	std::vector<requestJ>::iterator rit = requestBlocks.begin();
	while(rit != requestBlocks.end()) {
		if (rit->s <= dstar && rit->e >= dstar) {	//Does this job exist during dstar?
			rbAtDstar.push_back(*rit);
		}
		rit++;
	}
	//Our goal is now to find rho.
	float rho = (0.0f + rbAtDstar.begin()->size) / (min((widths[dstar] - cacheSize), cacheSize));
	//p2 for the first job is zero. Now we need to check that p2 didn't become negative for any other job at dstar.
	std::vector<requestJ>::iterator lrit = rbAtDstar.begin();	//Could probably reuse rit but I think this is less confusing.
	std::vector<requestJ>::iterator toBeRemoved = lrit;
	while (lrit != rbAtDstar.end()) {
		if (lrit->size - rho * min((widths[dstar] - cacheSize), cacheSize) < -0.0001f) {	//Is this p2 less than zero?
			float rho = (0.0f + lrit->size) / (min((widths[dstar] - cacheSize), cacheSize));	//Then update rho.
			toBeRemoved = lrit;
		}
	}
	//Now we have our rho for our dstar location. We need to remove that job, and go again from the top.
	removedBlocks.push_back(*toBeRemoved);
	//requestBlocks.
	//UNFINISHED - shifted to barNoy2() since some revamping is required.
}*/

int barNoy2() {
	int* widths;
	widths = new int[numFileAccesses];	//Each element of widths will have the total width of all jobs that occur in that time, NOT just how much overflow we have out of the cache.
	//First, create the requestJs and see how much bandwidth is being used at each time step.
	vector<requestJ> requestBlocks;
	vector<requestJ> removedBlocks;
	int tempJobID = 0;
	for (int i = 0; i < numFileAccesses; i++) {
		widths[i] = 0;
		int tempID = accessList[i];
		int k = i + 1;
		while (k != numFileAccesses) {
			if (accessList[k] == tempID) {
				break;
			}
			k++;
		}
		if (k != numFileAccesses) {
			requestJ *rj = new requestJ;
			rj->s = i;
			rj->e = k - 1;
			rj->JobFile = accessList[i];
			rj->size = uniqueFiles[accessList[i]].size;
			rj->JobID = tempJobID;
			tempJobID++;
			rj->penalty = (float)rj->size;
			requestBlocks.push_back(*rj);
		}
	}
	for (int j = 0; j < requestBlocks.size(); j++) {
		for (int k = requestBlocks[j].s; k <= requestBlocks[j].e; k++) {
			widths[k] += requestBlocks[j].size;
		}
	}
	//Now, we need to find where the largest element of widths is
	//cout << "Printing debug stuff for barnoy:\n";
	while (true) {
		//cout << "\n --------------------- \n";
		//Print out widths.
		//cout << "\nWidths:\n";
		/*for (int i = 0; i < numFileAccesses; i++) {
			cout << widths[i] << " ";
		}
		//Print out all jobs.
		cout << "\nAll Jobs:\n";
		for (int i = 0; i < requestBlocks.size(); i++) {
			cout << "JobID: " << requestBlocks[i].JobID << "\tJobFile: " << requestBlocks[i].JobFile << "\tFileSize: " << requestBlocks[i].size << "\tStart, end " << requestBlocks[i].s << " " << requestBlocks[i].e << " Penalty: " << requestBlocks[i].penalty << "\n";
		}*/
		int dstar = (std::max_element(widths, widths + numFileAccesses) - widths);	//Note that dstar is an INDEX.
		if (widths[dstar] <= cacheSize) {	//The schedule is now feasible.
			break;
		}
		//Now, we do the p2 thing, where P = rp1 + p2. Here, we consider the jobs that occur during dstar. The penalty function is just each job's size.
		//First, let's get a list of the jobs that occur at dstar.
		vector<requestJ> rbAtDstar;
		std::vector<requestJ>::iterator rit = requestBlocks.begin();
		while (rit != requestBlocks.end()) {
			if (rit->s <= dstar && rit->e >= dstar) {	//Does this job exist during dstar?
				rbAtDstar.push_back(*rit);
			}
			rit++;
		}
		//print out t star.
		//cout << "T star: " << dstar << "\n";
		//print out the jobs at t star.
		//cout << "Jobs at T star:\n";
		/*for (int i = 0; i < rbAtDstar.size(); i++) {
			cout << "JobID: " << rbAtDstar[i].JobID << "\tJobFile: " << rbAtDstar[i].JobFile << "\tFileSize: " << rbAtDstar[i].size << "\tStart, end: " << rbAtDstar[i].s << " " << rbAtDstar[i].e << " Pennalty: " << requestBlocks[i].penalty << "\n";
		}*/

		//Our goal is now to find rho.
		float rho = (0.0f + rbAtDstar.begin()->penalty) / (min((widths[dstar] - cacheSize), rbAtDstar.begin()->size));
		//p2 for the first job is zero. Now we need to check that p2 didn't become negative for any other job at dstar.
		std::vector<requestJ>::iterator lrit = rbAtDstar.begin();	//Could probably reuse rit but I think this is less confusing.
		std::vector<requestJ>::iterator toBeRemoved = lrit;
		while (lrit != rbAtDstar.end()) {
			if (lrit->penalty - rho * min((widths[dstar] - cacheSize), lrit->size) < -0.0001f) {	//Is this p2 less than zero?
				float rho = (0.0f + lrit->penalty) / (min((widths[dstar] - cacheSize), lrit->size));	//Then update rho.
				toBeRemoved = lrit;
			}
			lrit++;
		}
		//Update penalties.
		//lrit = rbAtDstar.begin();
		//while (lrit != rbAtDstar.end()) {
			//lrit->penalty = lrit->penalty - (rho * (min((widths[dstar] - cacheSize), lrit->size)));
			//lrit++;
		//}
		for (int i = 0; i < requestBlocks.size(); i++) {
			if (requestBlocks[i].s <= dstar && requestBlocks[i].e >= dstar) {
				requestBlocks[i].penalty -= rho * (min((widths[dstar] - cacheSize), requestBlocks[i].size));
			}
		}
		//Now we have our rho for our dstar location. We need to remove that job, and go again from the top.
		removedBlocks.push_back(*toBeRemoved);	//Add to the 'removed jobs' list.
		requestBlocks.erase(std::find(requestBlocks.begin(), requestBlocks.end(), *toBeRemoved));	//remove from request blocks.
		for (int k = toBeRemoved->s; k <= toBeRemoved->e; k++) {	//update widths.
			widths[k] -= toBeRemoved->size;
		}
		//cout << "Job we're getting rid of:\n" << "JobID: " << toBeRemoved->JobID << " JobFile: " << toBeRemoved->JobFile << " s, e: " << toBeRemoved->s << " " << toBeRemoved->e << " Penalty: " << toBeRemoved->penalty;
		rbAtDstar.clear();

	}
	// = removedBlocks.begin();
	if (removedBlocks.empty()) {
		//cout << "removed blocks was empty!";
	}
	else {
		std::vector<requestJ>::iterator backTrack = removedBlocks.end();
		backTrack--;
		while (backTrack >= removedBlocks.begin()) {
			bool reAdd = true;
			for (int k = backTrack->s; k <= backTrack->e; k++) {
				if (widths[k] + backTrack->size > cacheSize) {
					reAdd = false;
					break;
				}
			}
			if (reAdd) {
				requestBlocks.push_back(*backTrack);
				for (int k = backTrack->s; k <= backTrack->e; k++) {
					widths[k] += backTrack->size;
				}
			}
			if (backTrack == removedBlocks.begin()) {
				break;
			}
			backTrack--;
		}
		
	}
	int barCost = 0;
	for (int i = 0; i < numFileAccesses; i++) {
		barCost += uniqueFiles[accessList[i]].size;
	}
	for (int i = 0; i < requestBlocks.size(); i++) {
		barCost -= requestBlocks[i].size;
	}
	//We need to get the barnoy caches.
	string barnoyoutName = "BarNoyAlgo" + to_string(printTestCase) + ".txt";
	ofstream BnOut(barnoyoutName);
	if (WILLPRINT) {
		inputFilePrinter(BnOut);
		for (int i = 0; i < numFileAccesses; i++) {
			int csum = 0;
			for (int j = 0; j < requestBlocks.size(); j++) {
				if (requestBlocks[j].s < i && requestBlocks[j].e + 1 >= i) {
					//csum += requestBlocks[j].size;
					csum++;
				}
			}
			BnOut << csum << " ";
			for (int j = 0; j < requestBlocks.size(); j++) {
				if (requestBlocks[j].s < i && requestBlocks[j].e + 1>= i) {
					BnOut << requestBlocks[j].JobFile << " ";
				}
			}
			BnOut << "\n";
		}
	}
	if (WILLPRINT) {
		BnOut << barCost << "\n";
	}
	BnOut.close();


	/*cout << "\nWidths:";
	for (int i = 0; i < numFileAccesses; i++) {
		cout << " " << widths[i];
	}*/
	return barCost;
}

int lru() {
	string lruoutName = "LRUalgo" + to_string(printTestCase) + ".txt";
	ofstream LRUout(lruoutName);
	if (WILLPRINT) {
		inputFilePrinter(LRUout);
	}
	int lrucost = 0;
	int * dist = new int[numUniqueFiles];
	char * lruCache = new char[numUniqueFiles];
	for(int j = 0; j < numUniqueFiles; j++){
		dist[j] = 0;
		lruCache[j] = 'n';
	}
	for (int i = 0; i < numFileAccesses; i++) {
		//First, print stufff
		if (WILLPRINT) {
			int numfiles = 0;
			for (int r = 0; r < numUniqueFiles; r++) {
				if (lruCache[r] == 'y') {
					numfiles++;
				}
			}
			LRUout << numfiles << " ";
			for (int r = 0; r < numUniqueFiles; r++) {
				if (lruCache[r] == 'y') {
					LRUout << r << " ";
				}
			}
			LRUout << "\n";
		}
		//increment the dists of everything in the cache.
		for (int k = 0; k < numUniqueFiles; k++) {
			if (lruCache[k] == 'y') {
				dist[k]++;
			}
		}
		//Is file at accessList[i] in the cache?
		if (lruCache[accessList[i]] == 'y') {	//Yes
			dist[accessList[i]] = 0;
		}
		else {	//No. We have to add the current request to the cache.
			lrucost += uniqueFiles[accessList[i]].size;
			lruCache[accessList[i]] = 'y';
			dist[accessList[i]] = 0;
			//Make the sure the cache isn't overflowing.
			int lrusum = 0;
			for (int r = 0; r < numUniqueFiles; r++) {
				if (lruCache[r] == 'y') {
					lrusum += uniqueFiles[r].size;
				}
			}
			while (lrusum > cacheSize) {
				int leastRecentIndex = std::max_element(dist, dist + numUniqueFiles) - dist;
				lruCache[leastRecentIndex] = 'n';
				dist[leastRecentIndex] = 0;
				lrusum -= uniqueFiles[leastRecentIndex].size;
			}
		}
		
	}
	if (WILLPRINT) {
		LRUout << lrucost << "\n";
	}
	LRUout.close();
	return lrucost;
}

int main()
{
	//srand(time(NULL));
	srand(422);
	
	cout << "my directory is" << ExePath() << "\n";
	
	string dataOut = "projectdata.txt";
	ofstream doStream(dataOut);
	doStream << "Data:\n";
	int numTrials = 30;
	cacheSize = 100;
	FSizeL = 21;
	FSizeU = 40;
	int nuf[4] = { 5, 10, 100, 1000 };
	for (int i = 1; i >= 0 ; i--) {
		doStream << nuf[i] << "\n";
		cout << "Data for " << nuf[i] << " unique files:\n\n";
		numUniqueFiles = nuf[i];
		for (int j = 8; j >= 1; j--) {
			cout << "Doing " << (j * 500) << " accesses...\n";
			numFileAccesses = 500 * j;
			int HyAvg = 0;
			float HyRuntime = 0.0f;
			int HyFixAvg = 0;
			float HyFixRuntime = 0.0f;
			int HyVarAvg = 0;
			float HyVarRuntime = 0.0f;
			int WdAvg = 0;
			float WdRuntime = 0.0f;
			int BnAvg = 0;
			float BnRuntime = 0.0f;
			int LruAvg = 0;
			float LruRuntime = 0.0f;
			int mBruteAvg = 0;
			float mBruteRuntime = 0.0f;
			for (int k = 0; k < numTrials; k++) {
				cout << "\nTrial" << k;
				createRandUniqueFiles();
				createRandAccessList();

				clock_t t;
				t = clock();
				HyCacheAlgo();
				t = clock() - t;
				HyAvg += totalcost;
				totalcost = 0;
				HyRuntime += ((float)t) / CLOCKS_PER_SEC;

				t = clock() - t;
				HyCacheProxAlgo();
				t = clock() - t;
				HyFixAvg += totalcost;
				totalcost = 0;
				HyFixRuntime += ((float)t) / CLOCKS_PER_SEC;

				t = clock() - t;
				HyCacheVarWindow();
				t = clock() - t;
				HyVarAvg += totalcost;
				totalcost = 0;
				HyVarRuntime += ((float)t) / CLOCKS_PER_SEC;

				t = clock() - t;
				WdAvg += weightedDistAlgo();
				t = clock() - t;
				WdRuntime += ((float)t) / CLOCKS_PER_SEC;

				t = clock() - t;
				BnAvg += barNoy2();
				t = clock() - t;
				BnRuntime += ((float)t) / CLOCKS_PER_SEC;

				t = clock() - t;
				LruAvg += lru();
				t = clock() - t;
				LruRuntime += ((float)t) / CLOCKS_PER_SEC;

				if (nuf[i] <= 10) {
					t = clock() - t;
					buildHistoryMemo();
					bruteForceMemoAlgo();
					t = clock() - t;
					mBruteAvg += totalcostbrute;
					totalcostbrute = 0;
					mBruteRuntime += ((float)t) / CLOCKS_PER_SEC;
					freeHistory();
					history.clear();
				}
				cout << "\n" << HyAvg << " " << HyFixAvg << " " << BnAvg << " \n";

				//free(accessList);
				//free(uniqueFiles);
				delete accessList;
				delete uniqueFiles;
				printTestCase++;
			}
			doStream << (j * 500) << "\n";
			doStream << (HyAvg / numTrials) << " " << (HyFixAvg / numTrials) << " " << (HyVarAvg / numTrials) << " " << (WdAvg / numTrials) << " " << (BnAvg / numTrials) << " " << (LruAvg / numTrials) << " ";
			if (nuf[i] <= 10) {
				doStream << (mBruteAvg/numTrials) << " ";
			}
			doStream << "\t";
			doStream << (HyRuntime / numTrials) << " " << (HyFixRuntime / numTrials) << " " << (HyVarRuntime / numTrials) << " " << (WdRuntime / numTrials) << " " << (BnRuntime / numTrials) << " " << (LruRuntime / numTrials) << " ";
			if (nuf[i] <= 10) {
				doStream << (mBruteRuntime/numTrials) << " ";
			}
			
			//return 1;
			/*
			cout << "For " << (j * 500) << " File accesses, The average for Hy was " << (0.0f + HyAvg) / numTrials << "\n";
			cout << "For " << (j * 500) << " File accesses, The average for HyFix was " << (0.0f + HyFixAvg) / numTrials << "\n";
			cout << "For " << (j * 500) << " File accesses, The average for HyVar was " << (0.0f + HyVarAvg) / numTrials << "\n";
			cout << "For " << (j * 500) << " File accesses, The average for Wd was " << (0.0f + WdAvg) / numTrials << "\n";
			cout << "For " << (j * 500) << " File accesses, The average for Bn was " << (0.0f + BnAvg) / numTrials << "\n";
			cout << "For " << (j * 500) << " File accesses, The average for Lru was " << (0.0f + LruAvg) / numTrials << "\n";
			cout << "For " << (j * 500) << " File accesses, The average for bruteforce was " << (0.0f + LruAvg) / numTrials << "\n";
			*/
		}
	}
	doStream.close();
	_getch();
	return 1;
	

	int k = askForAlgoType();
	int n = askForInputType();
	if (n == 0) {
		getInputFromFile();
	}
	else {
		getInputForRand();
	}

	printAccessList();
	printUniqueFiles();
	
	cout << "\nWD cost was: " << weightedDistAlgo() << ".\n";
	//free(accessList);
	//free(uniqueFiles);
	delete accessList;
	delete uniqueFiles;
	_getch();
	return 1;
	
	switch (k) {
	case 0:		//only Hycache
		HyCacheAlgo();
		cout << "\nTotal cost via hycache algorithm was: " << totalcost << ".\n";
		break;
	case 1: //only brute force, non-memoized
		bruteForceAlgo();
		cout << "\nTotal cost via brute force algorithm was: " << totalcostbrute << ".\n";
		break;
	case 2: //only brute force, both memoized and nonmemoized
		buildHistoryMemo();
		bruteForceMemoAlgo();
		cout << "\nTotal cost via memoized brute force algorithm was: " << totalcostbrute << ".\n";
		totalcostbrute = 0;
		bruteForceAlgo();
		cout << "\nTotal cost via brute force algorithm was: " << totalcostbrute << ".\n";
		break;
	case 3: //only memoized brute force
		buildHistoryMemo();
		bruteForceMemoAlgo();
		cout << "\nTotal cost via memoized brute force algorithm was: " << totalcostbrute << ".\n";
		break;
	case 4: //Hycache, non-memoized brute
		HyCacheAlgo();
		bruteForceAlgo();
		cout << "\nTotal cost via brute force algorithm was: " << totalcostbrute << ".\n";
		cout << "\nTotal cost via hycache algorithm was: " << totalcost << ".\n";
		break;
	case 5: //Hycache and both brute force
		HyCacheAlgo();
		buildHistoryMemo();
		bruteForceMemoAlgo();
		cout << "\nTotal cost via memoized brute force algorithm was: " << totalcostbrute << ".\n";
		cout << "\nTotal cost via hycache algorithm was: " << totalcost << ".\n";
		totalcostbrute = 0;
		bruteForceAlgo();
		cout << "\nTotal cost via brute force algorithm was: " << totalcostbrute << ".\n";
		break;
	case 6: //Hycache and memoized brute
		HyCacheAlgo();
		cout << "\nTotal cost via hycache algorithm was: " << totalcost << ".\n";
		buildHistoryMemo();
		bruteForceMemoAlgo();
		cout << "\nTotal cost via memoized brute force algorithm was: " << totalcostbrute << ".\n";
		break;
	}
	
	cout << "\nWeightedThing was " << weightedDistAlgo() << ".\n";
	totalcost = 0;
	HyCacheVarWindow();
	cout << "\nTotal cost via hycachevarwindow was: " << totalcost << ".\n";
	totalcost = 0;
	HyCacheProxAlgo();
	cout << "\nTotal cost via hycacheprox algorithm was: " << totalcost << ".\n";
	totalcost = 0;
	HyCacheVarWindowMaxFile();
	cout << "\nAnd the total for HyVarWindowMaxFile was: " << totalcost << ".\n";
	totalcost = 0;
	HyCacheVarWindowSumFiles();
	cout << "For hyvarsumfiles, cost was: " << totalcost << ".\n";
	cout << "For Bar-Noy the cost was: " << barNoy2() << ".\n";
	/*
	if (n != 0) {
		cout << "Would you like to save the input data from this test? Type 'y' for yes and anything else for no.\n";
		char savechar;
		cin >> savechar;
		if (savechar == 'y') {
			sendInputToFile();
		}
	}*/
	//free(accessList);
	//free(uniqueFiles);
	delete accessList;
	delete uniqueFiles;
	if (needHistoryFreed) {
		freeHistory();
	}
	cout << "Press any key to exit...";
	_getch();
	exit(EXIT_SUCCESS);
	
	/*
	cout << "Current directory is" << ExePath() << "\n";
	int k = askForAlgoType();
	int n = askForInputType();
	
	std::unordered_map<std::unordered_set<int>, int, hash_intS> helpme;
	std::unordered_set<int> derp = { 0, 5 };
	//derp.push_back(0);
	//derp.push_back(5);
	std::unordered_set<int> pred = { 5, 0 };
	//pred.push_back(0);
	//pred.push_back(5);
	helpme[derp] = 0;
	helpme[pred] = 1;
	cout << "helpmederp is: " << helpme[derp] << " and pred is: " << helpme[pred] << ". ";
	_getch();*/
}

/*
// Example program
#include <iostream>
#include <string>
#include <vector>

struct requestJ {
int s;
int e;
int jID;
int size;
};

int main()
{
//std::string name;
//std::cout << "What is your name? ";
// getline (std::cin, name);
//std::cout << "Hello, " << name << "!\n";
std::vector<requestJ> myj;
for(int i = 0; i < 10; i++){
requestJ *rj = new requestJ;
rj->s = i;
rj->e = i;
rj->jID = i;
rj->size = i;
myj.push_back(*rj);
}
std::cout << "Original myj: ";
std::vector<requestJ>::iterator myji = myj.begin();
while(myji != myj.end()){
std::cout << myji->s << " ";
myji++;
}
myji = myj.begin();
std::vector<requestJ> oddints;
while(myji != myj.end()){
if(myji->s % 2 != 0){
oddints.push_back(*myji);
}
myji++;
}
myji = oddints.begin();
while(myji != oddints.end()){
myji->s = (myji->s) * 100;
myji++;
}
myji = myj.begin();
std::cout << "\nnew myj: ";
while(myji != myj.end()){
std::cout << myji->s << " ";
myji++;
}
myji = oddints.begin();
std::cout << "\noddints: ";
while(myji != oddints.end()){
std::cout << myji->s << " ";
myji++;
}
//std::cout << "\n" << myj.begin() << " " << oddints.begin();
myji = oddints.begin();
std::cout << "\noddints iterator on myj: ";
int k = 100;
while(myji != myj.end()){
std::cout << myji->s << " ";
myji++;
k--;
if(k == 0){
break;}
}
}

*/


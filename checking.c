using namespace std;
#include <iostream>
#include <fstream>

main()
{
	int i, j, k, kp, n, m, C, Sum, Cost, upload, Alg_cost;

	cin >> n >> m >> C;

	cout	<< "number of files, n = " << n << endl;
	cout	<< "number of requests, m = " << m << endl;
	cout	<< "C = " << C << endl;

	int Size[n];

	int Request[m];

	for (i=0; i < n; i++)
		cin >> Size[i];

	for (i=0; i < m; i++)
		cin >> Request[i];

	cout << "the input is: ";

	for (i=0; i < n; i++)
		cout << Size[i] << ' ';
	cout << endl;

	for (i=0; i < m; i++)
		cout << Request[i] << ' ';
	cout << endl;
	
	int A[n]; // previous files in cache
	int AA[n]; // bit vector of previous files
	int B[n]; // current files in cache
	for (i=0; i < n; i++)
		AA[i] = 0;
	cin >> kp; // expected to be 0; kp = previous cahce size
	Cost = 0;

	for (i = 1; i < m; i++)
	{
		cout << "Processing " << i << " which is " << Request [i-1] << " cache has ";
		cin  >> k;
		Sum = 0; //checking file sizes do not exceed cache;
		for (j=0; j<k; j++)
		{
			cin >> B[j];
			Sum = Sum + Size[B[j]];
			cout << B[j] << ' ';
		}

		if (Sum > C) 
			cout << "total size of files in cache too large" << endl;
		cout << "cache_load = " <<  Sum;

		// compute the cost:
		upload = 0;
		for(j=0; j < k; j++)
		{
			if ( B[j] == Request [i-1] )
				upload = 1;
			if ( AA[ B[j] ]  == 0 )
				Cost = Cost + Size[ B[j] ];
		}
		if (upload == 0)
		{
			cout << " not_up ";
			if ( AA[ Request[i-1] ]  == 0 )
				Cost = Cost + Size[ Request[i-1] ];
		}

		cout << " c_cost = " << Cost << endl;

		// reset AA and A and B 
		for (j=0; j < kp; j++)
			AA[ A[j] ] = 0 ;
		for(j=0; j < k; j++)
		{
			AA[ B[j] ] = 1;
			A[j] = B[j];	
		}
		kp = k;
	}

	// last request is different
	cout << "Processing " << m << " which is " << Request [m-1];
	if ( AA [ Request [m-1] ] == 0)
	{
		cout << " read_from_disk ";
		Cost = Cost + Size[ Request[i-1] ];

	}
	else
		cout << " no need to read! ";
	cout << " c_cost = " << Cost << endl;

	
	cin >> Alg_cost;
	cout << "as computed by the algorithm, cost is: " << Alg_cost << endl;

}

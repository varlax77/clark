/*
 *  CLARK, CLAssifier based on Reduced K-mers.
 */

/*
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    Copyright 2013-2017, Rachid Ounit <clark.ucr.help at gmail.com>
 */

/*
 *  @author: Rachid Ounit, Ph.D.
 *  @project: CLARK, Metagenomic and Genomic Sequences Classification project.
 *  @note: C++ IMPLEMENTATION supported on latest Linux and Mac OS.
 *  
 */

#include <cstdlib>
#include <iomanip>
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <map>
#include "file.hh"
#include <string>

using namespace std;

struct Organism
{
        string 	Name;
        size_t	Taxid;
        double 	RProportion;
	double	TProportion;

        Organism(): Name(""),Taxid(-1), RProportion(-1.0), TProportion(-1.0)
        {}

        bool operator<(const Organism& _o) const
        {       return (RProportion < _o.RProportion);  }
};

struct Sample
{
	string 	Name;
	int	NReads;
	int 	NRAssigned;
	vector<Organism> TopOrgs;
	
	Sample(): Name(""), NReads(0), NRAssigned(0)
	{}
	
	void Add(const Organism& o)
	{
		TopOrgs.push_back(o);
	}	
	
	void Display(const double& abd, ofstream& fout) const
	{
		fout << setprecision(4) <<  Name << "," << NReads << "," << NRAssigned << "," << abd << "," << ((double) NRAssigned)/((double) NReads)*100.0 << "%," ;
		for(size_t u = 0; u < TopOrgs.size(); u++)
		{
			if (TopOrgs[u].TProportion >= abd)	
			{	fout << TopOrgs[u].Name << "," << TopOrgs[u].Taxid << "," << TopOrgs[u].RProportion << "%," ;}
		}
		fout << std::endl;
	}

	void Update(const double& abd, std::map<size_t, size_t>& taxidToIdx, std::vector< Organism >& orgs, std::vector< std::vector<double> >& records) const
	{
		std::map<size_t, size_t>::iterator it;
		size_t idx = 0;
		for(size_t u = 0; u < TopOrgs.size(); u++)
                {
                        if (TopOrgs[u].TProportion >= abd)
                        {       
				it = taxidToIdx.find(TopOrgs[u].Taxid);
				if (it == taxidToIdx.end())
				{	idx = taxidToIdx.size();
					taxidToIdx[TopOrgs[u].Taxid] = idx;
					std::vector< double > tmp;
					records.push_back(tmp);
					orgs.push_back(TopOrgs[u]);
				}
				else
				{
					idx = it->second;
				}
				records[idx].push_back(TopOrgs[u].TProportion);	
			}
                }
	}
};

std::string getFileID(const char* _filename)
{
	string file(_filename);
	vector<char> sep;
	sep.push_back('/');
	vector<string> ele;
	getElementsFromLine(file, sep, ele);
	if (ele.size() <  1)
	{	std::cerr << "Failed to read the filename: " << _filename << std::endl; exit(1);	}
	return ele[ele.size()-1];
}

void getSummary(const char* filename, const int& topk, Sample& S)
{
	string 		line;
	vector<char> 	sep;
	vector<string> 	ele;
	sep.push_back(',');
	sep.push_back('\r');
	FILE * fd = fopen(filename, "r");
	if (fd == NULL)
	{	std::cerr << "Failed to open " << filename << endl;
		return;
	}
	int nbReads = 0, nbAssigned = 0;
	getLineFromFile(fd, line);
	
	int incr = (line.find("Lineage") != string::npos)?1:0;
	vector<Organism> Orgs;
	while (getLineFromFile(fd, line))
	{
		ele.clear();
		getElementsFromLine(line, sep, ele);
		if (ele[0] != "UNKNOWN")
		{
			Organism 	e;
			e.Name	 	= ele[0];
			e.Taxid 	= atoi(ele[1].c_str());
			e.RProportion 	= atof(ele[incr+4].c_str());
			e.TProportion	= atof(ele[incr+3].c_str());
			nbAssigned 	+= atoi(ele[incr+2].c_str());
			nbReads 	+= atoi(ele[incr+2].c_str());
			Orgs.push_back(e);
		}
		else
		{
			nbReads 	+= atoi(ele[3].c_str());
		}
	}
	fclose(fd);
	
	sort(Orgs.begin(), Orgs.end());
	S.Name 		= getFileID(filename);
	S.NReads	= nbReads;
	S.NRAssigned 	= nbAssigned;  
	for (size_t t = 0; t < topk && t < Orgs.size() ; t++)
	{	
		S.Add(Orgs[Orgs.size()-1-t]);
	}

	return;
}

void DisplaySamples(const int& topK, const double& abd, const vector<Sample>& Samples)
{
	ofstream fout("./TableSummary_per_Report.csv");
	fout << "SampleName,#Reads,#ClassifiedReads,MinAbundance(%),Proportion(%),";
	for(size_t i = 0; i < topK ; i++ )
        {
                fout << "Top" << i+1 << "_Taxon_Name,";
		fout << "Top" << i+1 << "_Taxon_Taxid,";
		fout << "Top" << i+1 << "_Taxon_RelProp(%),";
	}
	fout << std::endl;
	for(size_t i = 0; i < Samples.size() ; i++ )
	{
		Samples[i].Display(abd, fout);	
	}
	fout.close();
}

void DisplayOrgDistr(const double& abd, const vector<Sample>& Samples)
{
        std::map<size_t, size_t> TaxidToIdx;
	std::map<size_t, size_t>::iterator it;
	std::vector< Organism > Orgs;
	std::vector< std::vector< double > > Records;

	for(size_t i = 0; i < Samples.size() ; i++ )
        {
                Samples[i].Update(abd, TaxidToIdx, Orgs, Records);
        }
	ofstream fout("./TableSummary_per_Taxon.csv");
	fout << "Taxon_Name,Taxon_Taxid,MinAbundance(%),#ReportsFound,MinProportion(%),MaxProportion(%),AvgProportion(%),StdDevProportion(%),\n";	
	for(it = TaxidToIdx.begin(); it!= TaxidToIdx.end(); it++)
	{
		double avg = 0, stdDev = 0;
		double min = 1000.0, max = -1.0;
		for(size_t v = 0 ; v < Records[it->second].size(); v++)
		{
			if (Records[it->second][v] > max)
			{	max = Records[it->second][v];	}
			if (Records[it->second][v] < min)
                        {       min = Records[it->second][v];   }
			avg += Records[it->second][v];
		}
		avg /= ((double) Records[it->second].size());
		for(size_t v = 0 ; v < Records[it->second].size(); v++)
                {
                        stdDev += (Records[it->second][v] - avg) * (Records[it->second][v] - avg);
                }
		stdDev = sqrt(stdDev);
		stdDev /= ((double) Records[it->second].size());
		fout << Orgs[it->second].Name << "," << Orgs[it->second].Taxid << "," << abd << "," << Records[it->second].size() << "," << min <<"," << max << "," << avg << "," << stdDev << ",\n";
	}
	fout.close();
}

int main(const int argc, const char** argv)
{
	if (argc < 4)
	{	std::cerr << argv[0] << " <top r>  <min abundance in %> <CLARK Report filename1> <CLARK Report filename2> ... "<< std::endl; exit(1);}
	
	vector<Sample> Samples;
	double minAbundance = atof(argv[2]);
	for(size_t t = 3; t < argc; t++)
	{
		Sample S;
		getSummary(argv[t], atoi(argv[1]), S);
		if (S.Name == "" || (S.Name).size() < 1)
		{	continue;}
		Samples.push_back(S);
	}
	DisplaySamples(atoi(argv[1]), minAbundance, Samples);
	DisplayOrgDistr(minAbundance, Samples);
	return 0;
}


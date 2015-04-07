#include "Analysis.h"


using namespace std;
using namespace INRC2;


void solutionAnalysis( const std::string &logFileName, const std::string &outputFileName )
{
    struct Info
    {
    public:
        Info() : feasibleCount( 0 ), count( 0 ), obj( 0 ), feasibleObj( 0 ) {}

        string instance;
        string algorithm;
        double duration;
        int count;
        int feasibleCount;
        double obj;
        double feasibleObj;
    };

    vector<Info> instInfo;


    ifstream csvFile( logFileName );
    ofstream output( outputFileName );

    char buf[MaxLen::LINE];
    char c;

    // clear first line of header
    csvFile.getline( buf, MaxLen::LINE );
    // create header
    output << "Instance,Algorithm,Duration,InfeasibleCount,Obj,FeasibleObj" << endl;

    while (true) {
        string instance;
        string algorithm;
        double duration;
        bool feasible;
        double obj;

        csvFile.getline( buf, MaxLen::LINE, ',' );  // time
        csvFile.getline( buf, MaxLen::LINE, ',' );  // id
        csvFile.getline( buf, MaxLen::LINE, ',' );  // instance
        instance = buf;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // algorithm
        algorithm = buf;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // rand seed
        csvFile >> duration;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile >> feasible;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile.getline( buf, MaxLen::LINE, ',' );  // check obj
        csvFile >> obj >> c;
        csvFile.getline( buf, MaxLen::LINE );       // acc obj, solution

        if (csvFile.eof()) { break; }

        auto iter = instInfo.begin();
        for (; iter != instInfo.end(); ++iter) {
            if ((iter->instance == instance) && (iter->algorithm == algorithm)) {
                iter->duration += duration;
                iter->obj += obj;
                ++(iter->count);
                if (feasible) {
                    ++(iter->feasibleCount);
                    iter->feasibleObj += obj;
                }
                break;
            }
        }

        if (iter == instInfo.end()) {
            instInfo.push_back( Info() );
            instInfo.back().instance = instance;
            instInfo.back().algorithm = algorithm;
            instInfo.back().duration = duration;
            instInfo.back().count = 1;
            instInfo.back().feasibleCount = feasible;
            instInfo.back().obj = obj;
            instInfo.back().feasibleObj = feasible ? obj : 0;
        }
    }

    for (auto iter = instInfo.begin(); iter != instInfo.end(); ++iter) {
        output << iter->instance << ',' << iter->algorithm << ','
            << iter->duration / iter->count << ','
            << iter->count - iter->feasibleCount << ','
            << iter->obj / iter->count << ','
            << iter->feasibleObj / iter->feasibleCount << endl;
    }

    csvFile.close();
    output.close();
}

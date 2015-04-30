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
        double accObj;
    };

    vector<Info> instInfo;


    ifstream csvFile( logFileName );
    ofstream output( outputFileName );

    char buf[MaxLen::LINE];

    // clear first line of header
    csvFile.getline( buf, MaxLen::LINE );
    // create header
    output << "Instance,Algorithm,Duration,InfeasibleCount,Obj,FeasibleObj,FeasibleAccObj" << endl;

    while (true) {
        string instance;
        string algorithm;
        double duration;
        NurseRostering::ObjValue feasible;
        double obj;
        double accObj;

        csvFile.getline( buf, MaxLen::LINE, ',' );  // time
        csvFile.getline( buf, MaxLen::LINE, ',' );  // id
        csvFile.getline( buf, MaxLen::LINE, ',' );  // instance
        instance = buf;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // algorithm
        algorithm = buf;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // rand seed
        csvFile.getline( buf, MaxLen::LINE, ',' );  // generation count
        csvFile.getline( buf, MaxLen::LINE, ',' );  // iteration count
        csvFile >> duration;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile >> feasible;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile.getline( buf, MaxLen::LINE, ',' );  // check obj
        csvFile >> obj;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile >> accObj;                          // acc obj
        csvFile.getline( buf, MaxLen::LINE );       // solution

        if (csvFile.eof()) { break; }

        auto iter = instInfo.begin();
        for (; iter != instInfo.end(); ++iter) {
            if ((iter->instance == instance) && (iter->algorithm == algorithm)) {
                iter->duration += duration;
                iter->obj += obj;
                ++(iter->count);
                if (feasible == 0) {
                    ++(iter->feasibleCount);
                    iter->feasibleObj += obj;
                    iter->accObj += accObj;
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
            instInfo.back().feasibleCount = (feasible == 0);
            instInfo.back().obj = obj;
            instInfo.back().feasibleObj = (feasible == 0) ? obj : 0;
            instInfo.back().accObj = (feasible == 0) ? accObj : 0;
        }
    }

    for (auto iter = instInfo.begin(); iter != instInfo.end(); ++iter) {
        output << iter->instance << ',' << iter->algorithm << ','
            << iter->duration / iter->count << ','
            << iter->count - iter->feasibleCount << ','
            << iter->obj / iter->count << ','
            << iter->feasibleObj / iter->feasibleCount << ','
            << iter->accObj / iter->feasibleCount << endl;
    }

    csvFile.close();
    output.close();
}

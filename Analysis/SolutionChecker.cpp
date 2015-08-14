#include "Analysis.h"


using namespace std;
using namespace INRC2;


const std::string validatorResultName( "Validator-results.txt" );
const std::string H1Name( "Minimal coverage constraints: " );
const std::string H2name( "Required skill constraints: " );
const std::string H3name( "Illegal shift type succession constraints: " );
const std::string H4name( "Single assignment per day: " );
const std::string totalObjName( "Total cost: " );

static const string slnFileNameSuffix( "sol.txt" );


NurseRostering::ObjValue rebuildSolution( const string &logFileName, const string &logTime, const string id, ValidatorArgvPack &vap )
{
    makeSureDirExist( outputDirPrefix );

    ifstream csvFile( logFileName );

    char timeBuf[MaxLen::LINE];
    char idBuf[MaxLen::LINE];
    char instNameBuf[MaxLen::INST_NAME];
    char hisNameBuf[MaxLen::INIT_HIS_NAME];
    char weekdataNameBuf[MaxLen::WEEKDATA_NAME];
    char buf[MaxLen::ALGORITHM_NAME];
    char c;

    int feasible;
    double checkObj;
    double obj;
    double accObj;

    // locate first stage of the run
    do {
        csvFile.getline( timeBuf, MaxLen::LINE );   // clear last line
        csvFile.getline( timeBuf, MaxLen::LINE, ',' );
        csvFile.getline( idBuf, MaxLen::LINE, ',' );
        if (csvFile.eof()) {
            csvFile.close();
            return -1;
        }
    } while (!((logTime == timeBuf) && (id == idBuf)));

    // read information
    csvFile.getline( instNameBuf, MaxLen::INST_NAME, '[' );
    csvFile.getline( hisNameBuf, MaxLen::INIT_HIS_NAME, ']' );
    csvFile.get();  // '['
    csvFile.getline( weekdataNameBuf, MaxLen::WEEKDATA_NAME, ']' );
    csvFile.get();  // ','

    csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // algorithmName
    csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // randSeed
    csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // generation count
    csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // iteration count
    csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // timeCost
    csvFile >> feasible >> c >> checkObj >> c
        >> obj >> c >> accObj >> c;

    vap.instName = instNameBuf;
    vap.sceName = instanceDir + vap.instName + scePrefix + vap.instName + fileSuffix;
    vap.initHisName = instanceDir + vap.instName + '/' + hisNameBuf;

    NurseRostering problem;
    if (!readScenario( vap.sceName, problem )) {
        csvFile.close();
        return -1;
    }

    NurseRostering::AssignTable assign( problem.scenario.nurseNum );
    double totalObj = obj;

    for (NurseRostering::NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = NurseRostering::Weekday::Mon; weekday <= NurseRostering::Weekday::Sun; ++weekday) {
            csvFile >> assign[nurse][weekday].shift >> assign[nurse][weekday].skill;
        }
    }

    char week = '0';
    problem.history.pastWeekCount = 0;

    vap.weekdataName.push_back( instanceDir + vap.instName + '/' + weekdataNameBuf );
    vap.solFileName.push_back( outputDirPrefix + '/' + week + slnFileNameSuffix );
    writeSolution( vap.solFileName.back(),
        NurseRostering::TabuSolver( problem, NurseRostering::Output( static_cast<NurseRostering::ObjValue>(obj), assign ) ) );

    // handle rest weeks
    for (++problem.history.pastWeekCount, ++week;
        problem.history.pastWeekCount < problem.scenario.totalWeekNum;
        ++problem.history.pastWeekCount, ++week) {
        // locate the line
        do {
            csvFile.getline( timeBuf, MaxLen::LINE );   // clear last line
            csvFile.getline( timeBuf, MaxLen::LINE, ',' );
            csvFile.getline( idBuf, MaxLen::LINE, ',' );
            if (csvFile.eof()) {
                csvFile.close();
                return -1;
            }
        } while (id != idBuf);

        // read information
        csvFile.getline( instNameBuf, MaxLen::INST_NAME, '[' );
        csvFile.getline( hisNameBuf, MaxLen::INIT_HIS_NAME, ']' );
        csvFile.get();  // '['
        csvFile.getline( weekdataNameBuf, MaxLen::WEEKDATA_NAME, ']' );
        csvFile.get();  // ','
        if ((vap.instName != instNameBuf)
            || ((hisNameBuf[0] == 'H') && (hisNameBuf[1] == '0'))) {
            csvFile.close();
            return -1;
        }

        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // algorithmName
        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // randSeed
        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // generation count
        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // iteration count
        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // timeCost
        csvFile >> feasible >> c >> checkObj >> c
            >> obj >> c >> accObj >> c;
        totalObj += obj;

        for (NurseRostering::NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            for (int weekday = NurseRostering::Weekday::Mon; weekday <= NurseRostering::Weekday::Sun; ++weekday) {
                csvFile >> assign[nurse][weekday].shift >> assign[nurse][weekday].skill;
            }
        }

        vap.weekdataName.push_back( instanceDir + vap.instName + '/' + weekdataNameBuf );
        vap.solFileName.push_back( outputDirPrefix + '/' + week + slnFileNameSuffix );
        writeSolution( vap.solFileName.back(),
            NurseRostering::TabuSolver( problem, NurseRostering::Output( static_cast<NurseRostering::ObjValue>(obj), assign ) ) );
    }

    csvFile.close();
    return static_cast<NurseRostering::ObjValue>(totalObj);
}

int callValidator( const ValidatorArgvPack &vap )
{
    string argv( "java -jar " + instanceDir + "validator.jar" );
    argv += (" --sce " + vap.sceName);
    argv += (" --his " + vap.initHisName);
    argv += " --weeks";
    for (auto iter = vap.weekdataName.begin(); iter != vap.weekdataName.end(); ++iter) {
        argv += (" " + *iter);
    }
    argv += " --sols";
    for (auto iter = vap.solFileName.begin(); iter != vap.solFileName.end(); ++iter) {
        argv += (" " + *iter);
    }
    argv += (" 1> " + validatorResultName + " 2>&1 ");

    return system( argv.c_str() );
}

void validatorCheck( const std::string &logFileName, const std::string &outputFileName )
{
    ifstream csvFile( logFileName );
    ofstream output( outputFileName );

    char buf[MaxLen::LINE];
    char timeBuf[MaxLen::LINE];
    char idBuf[MaxLen::INST_NAME];
    char instNameBuf[MaxLen::INST_NAME];


    // clear first line of header
    csvFile.getline( timeBuf, MaxLen::LINE );
    // create header
    output << "Time,ID,Instance,Seed,Obj,Feasible,Validator-Obj" << endl;

    while (true) {
        csvFile.getline( timeBuf, MaxLen::LINE, ',' );
        csvFile.getline( idBuf, MaxLen::INST_NAME, ',' );
        csvFile.getline( instNameBuf, MaxLen::INST_NAME, '[' );
        csvFile.getline( buf, MaxLen::LINE, ',' );    // instance
        csvFile.getline( buf, MaxLen::LINE, ',' );    // algorithmName
        csvFile.getline( buf, MaxLen::LINE, ',' );    // rand seed
        if (csvFile.eof()) { break; }

        ValidatorArgvPack vap;
        NurseRostering::ObjValue totalObj = rebuildSolution( logFileName, timeBuf, idBuf, vap );
        if (totalObj >= 0) {
            callValidator( vap );

            NurseRostering::ObjValue checkResult = getObjValueInValidatorResult();
            output << timeBuf << "," << idBuf << ","
                << instNameBuf << "," << buf << "," << totalObj << ","
                << (checkResult >= 0) << "," << (checkResult - totalObj) << endl;
        }

        csvFile.getline( buf, MaxLen::LINE );    // clear line
    }

    csvFile.close();
    output.close();
}

NurseRostering::ObjValue getObjValueInValidatorResult()
{
    NurseRostering::ObjValue totalObj = -1;
    ifstream validatorResult( validatorResultName );

    char buf[MaxLen::LINE];
    istringstream iss;

    do {
        validatorResult.getline( buf, MaxLen::LINE );
        if (validatorResult.eof()) {
            validatorResult.close();
            return -1;
        }
    } while (strstr( buf, H1Name.c_str() ) == NULL);
    iss.clear();
    iss.str( buf + H1Name.size() );
    iss >> totalObj;
    if (totalObj != 0) {
        validatorResult.close();
        return -1;
    }

    do {
        validatorResult.getline( buf, MaxLen::LINE );
        if (validatorResult.eof()) {
            validatorResult.close();
            return -1;
        }
    } while (strstr( buf, H2name.c_str() ) == NULL);
    iss.clear();
    iss.str( buf + H2name.size() );
    iss >> totalObj;
    if (totalObj != 0) {
        validatorResult.close();
        return -1;
    }

    do {
        validatorResult.getline( buf, MaxLen::LINE );
        if (validatorResult.eof()) {
            validatorResult.close();
            return -1;
        }
    } while (strstr( buf, H3name.c_str() ) == NULL);
    iss.clear();
    iss.str( buf + H3name.size() );
    iss >> totalObj;
    if (totalObj != 0) {
        validatorResult.close();
        return -1;
    }

    do {
        validatorResult.getline( buf, MaxLen::LINE );
        if (validatorResult.eof()) {
            validatorResult.close();
            return -1;
        }
    } while (strstr( buf, H4name.c_str() ) == NULL);
    iss.clear();
    iss.str( buf + H4name.size() );
    iss >> totalObj;
    if (totalObj != 0) {
        validatorResult.close();
        return -1;
    }

    do {
        validatorResult.getline( buf, MaxLen::LINE );
        if (validatorResult.eof()) {
            validatorResult.close();
            return -1;
        }
    } while (strstr( buf, totalObjName.c_str() ) == NULL);
    iss.clear();
    iss.str( buf + totalObjName.size() );
    iss >> totalObj;

    validatorResult.close();
    return totalObj;
}

void analyzeCheckResult( const std::string &checkFileName, const std::string &outputFileName )
{
    struct Info
    {
    public:
        Info() : count( 0 ), objSum( 0 ), minObj( NurseRostering::MAX_OBJ_VALUE ) {}

        string instance;
        int seed;
        int count;
        double objSum;
        double minObj;
    };

    vector<Info> instInfo;


    ifstream csvFile( checkFileName );
    ofstream output( outputFileName );

    char buf[MaxLen::LINE];

    // clear first line of header
    csvFile.getline( buf, MaxLen::LINE );
    // create header
    output << "Instance,AvgObj,MinObj,Seed" << endl;

    while (true) {
        string instance;
        int seed;
        NurseRostering::ObjValue feasible;
        double obj;

        csvFile.getline( buf, MaxLen::LINE, ',' );  // time
        csvFile.getline( buf, MaxLen::LINE, ',' );  // id
        csvFile.getline( buf, MaxLen::LINE, ',' );  // instance
        instance = buf;
        csvFile >> seed;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile >> obj;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile >> feasible;
        csvFile.getline( buf, MaxLen::LINE, ',' );  // clear
        csvFile.getline( buf, MaxLen::LINE );       // clear line

        if (csvFile.eof()) { break; }

        if (feasible) {
            auto iter = instInfo.begin();
            for (; iter != instInfo.end(); ++iter) {
                if (iter->instance == instance) {
                    iter->objSum += obj;
                    if (obj < iter->minObj) {
                        iter->minObj = obj;
                        iter->seed = seed;

                    }
                    ++(iter->count);
                    break;
                }
            }

            if (iter == instInfo.end()) {
                instInfo.push_back( Info() );
                instInfo.back().instance = instance;
                instInfo.back().seed = seed;
                instInfo.back().count = 1;
                instInfo.back().objSum = obj;
                instInfo.back().minObj = obj;
            }
        }
    }

    for (auto iter = instInfo.begin(); iter != instInfo.end(); ++iter) {
        output << iter->instance << ',' << iter->objSum / iter->count << ','
            << iter->minObj << ',' << iter->seed << endl;
    }

    csvFile.close();
    output.close();
}
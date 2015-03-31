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


NurseRostering::ObjValue rebuildSolution( const string &logFileName, const string &logTime, const string id, ValidatorArgvPack &argv )
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

    bool feasible;
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
    csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // timeCost
    csvFile >> feasible >> c >> checkObj >> c
        >> obj >> c >> accObj >> c;

    argv.instName = instNameBuf;
    argv.sceName = instanceDir + argv.instName + scePrefix + argv.instName + fileSuffix;
    argv.initHisName = instanceDir + argv.instName + '/' + hisNameBuf;

    NurseRostering problem;
    if (!readScenario( argv.sceName, problem )) {
        csvFile.close();
        return -1;
    }

    NurseRostering::AssignTable assign( problem.scenario.nurseNum );
    double totalObj = checkObj;

    for (NurseRostering::NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = NurseRostering::Weekday::Mon; weekday < NurseRostering::Weekday::SIZE; ++weekday) {
            csvFile >> assign[nurse][weekday].shift >> assign[nurse][weekday].skill;
        }
    }

    char week = '0';
    problem.history.pastWeekCount = 0;

    argv.weekdataName.push_back( instanceDir + argv.instName + '/' + weekdataNameBuf );
    argv.solFileName.push_back( outputDirPrefix + '/' + week + slnFileNameSuffix );
    writeSolution( argv.solFileName.back(),
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
        if (argv.instName != instNameBuf) {
            csvFile.close();
            return -1;
        }
        csvFile.getline( hisNameBuf, MaxLen::INIT_HIS_NAME, ']' );
        csvFile.get();  // '['
        csvFile.getline( weekdataNameBuf, MaxLen::WEEKDATA_NAME, ']' );
        csvFile.get();  // ','

        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // algorithmName
        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // randSeed
        csvFile.getline( buf, MaxLen::ALGORITHM_NAME, ',' );    // timeCost
        csvFile >> feasible >> c >> checkObj >> c
            >> obj >> c >> accObj >> c;
        totalObj += checkObj;

        for (NurseRostering::NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            for (int weekday = NurseRostering::Weekday::Mon; weekday < NurseRostering::Weekday::SIZE; ++weekday) {
                csvFile >> assign[nurse][weekday].shift >> assign[nurse][weekday].skill;
            }
        }

        argv.weekdataName.push_back( instanceDir + argv.instName + '/' + weekdataNameBuf );
        argv.solFileName.push_back( outputDirPrefix + '/' + week + slnFileNameSuffix );
        writeSolution( argv.solFileName.back(),
            NurseRostering::TabuSolver( problem, NurseRostering::Output( static_cast<NurseRostering::ObjValue>(obj), assign ) ) );
    }

    csvFile.close();
    return static_cast<NurseRostering::ObjValue>(totalObj);
}

void validatorCheck( const std::string &logFileName, const std::string &outputFileName )
{
    ifstream csvFile( logFileName );
    ofstream output( outputFileName );

    char timeBuf[MaxLen::LINE];
    char idBuf[MaxLen::LINE];

    // clear first line of header
    csvFile.getline( timeBuf, MaxLen::LINE );
    // create header
    output << "Time,ID,Feasible,Validator-Obj" << endl;

    while (true) {
        csvFile.getline( timeBuf, MaxLen::LINE, ',' );
        csvFile.getline( idBuf, MaxLen::LINE, ',' );
        if (csvFile.eof()) { break; }

        ValidatorArgvPack varg;
        NurseRostering::ObjValue totalObj = rebuildSolution( logFileName, timeBuf, idBuf, varg );
        if (totalObj >= 0) {
            string argv( "java -jar " + instanceDir + "validator.jar" );
            argv += (" --sce " + varg.sceName);
            argv += (" --his " + varg.initHisName);
            argv += " --weeks";
            for (auto iter = varg.weekdataName.begin(); iter != varg.weekdataName.end(); ++iter) {
                argv += (" " + *iter);
            }
            argv += " --sols";
            for (auto iter = varg.solFileName.begin(); iter != varg.solFileName.end(); ++iter) {
                argv += (" " + *iter);
            }
            argv += (" 1> " + validatorResultName + " 2>&1 ");

            int r = system( argv.c_str() );

            NurseRostering::ObjValue checkResult = getObjValueInValidatorResult();
            output << timeBuf << "," << idBuf << "," << (checkResult >= 0) << "," << (checkResult - totalObj) << endl;
        }

        csvFile.getline( idBuf, MaxLen::LINE );    // clear line
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

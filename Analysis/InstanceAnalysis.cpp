#include "Analysis.h"


using namespace std;
using namespace INRC2;


void analyzeInstance()
{
    const string header1_prefix0( "Scenario,Min|Max|Limit,AvgNurse" );
    const string header1_prefix1( "Scenario,AvgNursePerWeek,PerWeekend" );
    const string header1_suffix( ",Avg|Ideal Nurse" );
    const string header2_prefix0( "WeekData,Min|Opt,Min|Opt" );
    const string header2_prefix1( "WeekData,TotalRequire,WeekendRequire" );
    const string header2_suffix( ",Min|Opt RequirePerSkill" );

    ofstream csv( "InstanceAnalysis.csv" );

    for (unsigned i = 0; i < instance.size(); i++) {
        NurseRostering p;

        // print scenario header
        string scenarioName( instanceDir + instance[i] + scePrefix + instance[i] + fileSuffix );
        readScenario( scenarioName, p );
        csv << header1_prefix0;
        for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
            csv << header1_suffix;
        }
        csv << endl << header1_prefix1 << ',';
        for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
            csv << p.names.skillNames[sk] << ',';
        }
        csv << endl;
        // analyze scenario info
        int minNurseNum = 0;
        int maxNurseNum = 0;
        int maxNurseOnWeekend = 0;
        int totalNurseNum = p.scenario.nurseNum * 7;
        vector<int> nurseNumPerSkill( p.scenario.skillSize, 0 );
        vector<double> averageNurseNumPerSkill( p.scenario.skillSize, 0 );
        for (int n = 0; n < p.scenario.nurseNum; n++) {
            minNurseNum += p.scenario.contracts[p.scenario.nurses[n].contract].minShiftNum;
            maxNurseNum += p.scenario.contracts[p.scenario.nurses[n].contract].maxShiftNum;
            maxNurseOnWeekend += p.scenario.contracts[p.scenario.nurses[n].contract].maxWorkingWeekendNum;
            const vector<bool> &skills = p.scenario.nurses[n].skills;
            for (int skill = NurseRostering::Scenario::Skill::ID_BEGIN; skill < p.scenario.skillSize; skill++) {
                if (skills[skill]) {
                    nurseNumPerSkill[skill]++;
                    averageNurseNumPerSkill[skill] += (1.0 / skills.size());
                }
            }
        }
        // print scenario info
        csv << instance[i] << ','
            << minNurseNum / (p.scenario.maxWeekCount + 1.0)
            << '|' << maxNurseNum / (p.scenario.maxWeekCount + 1.0)
            << '|' << totalNurseNum << ',' << maxNurseOnWeekend * 2 / (p.scenario.maxWeekCount + 1.0);
        for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
            csv << ',' << averageNurseNumPerSkill[sk] * 7 << '|' << nurseNumPerSkill[sk] * 7;
        }
        csv << endl;

        // print weekdata header
        csv << header2_prefix0;
        for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
            csv << header2_suffix;
        }
        csv << endl << header2_prefix1 << ',';
        for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
            csv << p.names.skillNames[sk] << ',';
        }
        csv << endl;
        // analyze weekdata info
        for (int w = 0; w < WEEKDATA_NUM; w++) {
            ostringstream weekdataName;
            weekdataName << instanceDir << instance[i] << weekPrefix << instance[i]
                << '-' << w << fileSuffix;
            readWeekData( weekdataName.str(), p );

            int totalMinNurseRequire = 0;
            int totalOptNurseRequire = 0;
            int minNurseRequireOnWeekend = 0;
            int optNurseRequireOnWeekend = 0;
            vector<int> minRequirePerSkill( p.scenario.skillSize, 0 );
            vector<int> optRequirePerSkill( p.scenario.skillSize, 0 );
            for (int weekday = NurseRostering::Weekday::Mon; weekday < NurseRostering::Weekday::SIZE; ++weekday) {
                for (int sh = NurseRostering::Scenario::Shift::ID_BEGIN; sh < p.scenario.shiftSize; sh++) {
                    for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
                        int minn = p.weekData.minNurseNums[weekday][sh][sk];
                        int optn = p.weekData.optNurseNums[weekday][sh][sk];
                        totalMinNurseRequire += minn;
                        totalOptNurseRequire += optn;
                        minRequirePerSkill[sk] += minn;
                        optRequirePerSkill[sk] += optn;
                        if (weekday >= NurseRostering::Weekday::Sat) {
                            minNurseRequireOnWeekend += minn;
                            optNurseRequireOnWeekend += optn;
                        }
                    }
                }
            }
            // print weekdata info
            csv << w << ',' << totalMinNurseRequire << '|' << totalOptNurseRequire << ','
                << minNurseRequireOnWeekend << '|' << optNurseRequireOnWeekend;
            for (int sk = NurseRostering::Scenario::Skill::ID_BEGIN; sk < p.scenario.skillSize; sk++) {
                csv << ',' << minRequirePerSkill[sk] << '|' << optRequirePerSkill[sk];
            }
            csv << endl;
        }
        csv << endl;
    }

    csv.close();
}
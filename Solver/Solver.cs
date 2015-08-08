// TODO[9]: mark methods as fixed time or contain infinite loop.
// TODO[5]: record distance to optima after every move, analyze if it is circling around.
// TODO[4]: use polymorphisms on Move to replace TryMove, ApplyMove and UpdateTabu delegate
//          and simplify invalidateCacheFlag().


#region performance switch
// comment to reduce error messages and log
#define INRC2_LOG

// comment to reduce unnecessary calculation
#define INRC2_DEBUG

// comment to reduce console output
#define INRC2_PERFORMANCE_TEST
#endregion performance switch

#region input switch
// uncomment to check the feasibility of the instance from the online feasible checker
//#define INRC2_CHECK_INSTANCE_FEASIBILITY_ONLINE
#endregion input switch

#region algorithm switch
// comment to abandon tabu
#define INRC2_USE_TABU  // repair() has simple move only, will no tabu effect its execution?

// [fix] comment to start to consider min shift at the first week
#define INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS

// comment to use rest max working weekend
#define INRC2_AVERAGE_MAX_WORKING_WEEKEND

// comment to use rest max shift number
#define INRC2_AVERAGE_TOTAL_SHIFT_NUM

// [fix] comment to use random pick
#define INRC2_SECONDARY_OBJ_VALUE

// [fix] comment to use a fixed delta of perturb strength 
#define INRC2_INC_PERTURB_STRENGTH_DELTA

// [fix] comment to use perturb
#define INRC2_PERTRUB_IN_REBUILD

// [fix] uncomment to search all neighborhood after update optima
//#define INRC2_LS_AFTER_TSR_UPDATE_OPT

// comment to use best improvement
//#define INRC2_BLOCK_SWAP_FIRST_IMPROVE

// tabu strength of block swap
#if INRC2_USE_TABU
#define INRC2_BLOCK_SWAP_AVERAGE_TABU
//#define INRC2_BLOCK_SWAP_STRONG_TABU
//#define INRC2_BLOCK_SWAP_WEAK_TABU
//#define INRC2_BLOCK_SWAP_NO_TABU
#else // INRC2_USE_TABU  // must be no tabu if global setting is no tabu
#define INRC2_BLOCK_SWAP_NO_TABU
#endif // INRC2_USE_TABU

// [fix] grain of block swap
//#define INRC2_BLOCK_SWAP_BASE
//#define INRC2_BLOCK_SWAP_FAST
//#define INRC2_BLOCK_SWAP_PART
//#define INRC2_BLOCK_SWAP_RAND
#define INRC2_BLOCK_SWAP_CACHED

#endregion algorithm switch


using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Diagnostics;


namespace NurseRostering
{
    #region type alias
    // TUNEUP[2]: Int32 of ShiftID, SkillID and NurseID to Int16? (speed up AssignTable copying)
    using ContractID = Int32;
    using Duration = Int64;     // in milliseconds
    using TimePoint = Int64;    // in ticks
    using IterCount = Int32;
    using NurseID = Int32;
    using ObjValue = Int32;
    using SecondaryObjValue = Double;
    using ShiftID = Int32;
    using SkillID = Int32;
    using Weekday = Int32;
    #endregion type alias

    #region constants
    /// <summary> time in milliseconds. </summary>
    public static class Durations
    {
        public const Duration Max = (1 << 30);

        /// <summary> time in a second. </summary>
        public static readonly double Frequency = Stopwatch.Frequency;
        public static readonly double MillisecondsInSecond = 1000.0;

        public const string TimeFormat_DigitOnly = "yyyyMMddHHmmss";
        public const string TimeFormat_Readable = "yyyy-MM-dd HH:mm:ss";
    }

    public static class IterCounts
    {
        ///	<summary> default value for maxIterCount. </summary>
        public const IterCount Max = (1 << 30);
    }

    public static class NurseIDs
    {
        public const NurseID None = Begin - 1;
        public const NurseID Begin = default(NurseID);
    }

    public static class ShiftIDs
    {
        public const ShiftID Any = None - 1;
        /// <summary> the default value makes it no assignment. </summary>
        public const ShiftID None = default(ShiftID);
        public const ShiftID Begin = None + 1;
        public const string AnyStr = "Any";
        public const string NoneStr = "None";

        public static bool isWorking(ShiftID shift) {
            return (shift != ShiftIDs.None);
        }
    }

    public static class SkillIDs
    {
        /// <summary> the default value makes it no assignment. </summary>
        public const SkillID None = default(SkillID);
        public const SkillID Begin = None + 1;
    }

    public static class ContractIDs
    {
        public const ContractID None = Begin - 1;
        public const ContractID Begin = default(ContractID);
    }

    public static class Weekdays
    {
        public const Weekday LastWeek = 0;          // sentinel for history and succession
        public const Weekday Mon = LastWeek + 1;
        public const Weekday Tue = Mon + 1;
        public const Weekday Wed = Tue + 1;
        public const Weekday Thu = Wed + 1;
        public const Weekday Fri = Thu + 1;
        public const Weekday Sat = Fri + 1;
        public const Weekday Sun = Sat + 1;
        public const Weekday NextWeek = Sun + 1;    // sentinel for succession

        /// <summary> weekday bound for common use. </summary>
        public const Weekday Length = Sun + 1;
        /// <summary> take all sentinel into consideration (speed up succession check). </summary>
        public const Weekday Length_Full = NextWeek + 1;
        /// <summary> number of days in a week. </summary>
        public const Weekday Num = Sun - Mon + 1;

        public static readonly string[] Names = new string[] {
            null, "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
        };

        public static readonly Dictionary<string, Weekday> Map =
            new Dictionary<string, Weekday> {
            { Names[Mon], Mon },
            { Names[Tue], Tue },
            { Names[Wed], Wed },
            { Names[Thu], Thu },
            { Names[Fri], Fri },
            { Names[Sat], Sat },
            { Names[Sun], Sun }
        };
    }

    public static class DefaultPenalty
    {
        /// <summary> (delta >= MAX_OBJ_MAX) stands for forbidden move. </summary>
        public const ObjValue MaxObjValue = (1 << 24);
        /// <summary> tryMove should return ForbiddenMove in case it is in a complex move. </summary>
        public const ObjValue ForbiddenMove = (MaxObjValue * 2);
        /// <summary> amplifier for improving accuracy. </summary>
        public const ObjValue Amp = 2 * 2 * 2 * 3 * 7;

        #region hard constraints
        public const ObjValue SingleAssign = ForbiddenMove;
        public const ObjValue Understaff = ForbiddenMove;
        public const ObjValue Succession = ForbiddenMove;
        public const ObjValue MissSkill = ForbiddenMove;

        // TUNEUP[3]: what ratio will make the repair easier?
        public const ObjValue Understaff_Repair = (Amp * 80);
        public const ObjValue Succession_Repair = (Amp * 120);
        public const ObjValue MissSkill_Repair = (Amp * 160);
        #endregion hard constraints

        #region soft constraints
        public const ObjValue InsufficientStaff = (Amp * 30);
        public const ObjValue ConsecutiveShift = (Amp * 15);
        public const ObjValue ConsecutiveDay = (Amp * 30);
        public const ObjValue ConsecutiveDayOff = (Amp * 30);
        public const ObjValue Preference = (Amp * 10);
        public const ObjValue CompleteWeekend = (Amp * 30);
        public const ObjValue TotalAssign = (Amp * 20);
        public const ObjValue TotalWorkingWeekend = (Amp * 30);
        #endregion soft constraints
    }
    #endregion constants

    #region input
    /// <summary> original data from instance files. </summary>
    [DataContract]
    public class Input_INRC2Json
    {
        #region Constructor
        #endregion Constructor

        #region Method
        public static T readFile<T>(string path) {
            using (FileStream fs = File.Open(path,
                FileMode.Open, FileAccess.Read, FileShare.Read)) {
                return deserializeJsonStream<T>(fs);
            }
        }

        public static T deserializeJsonStream<T>(Stream stream) {
            DataContractJsonSerializer js = new DataContractJsonSerializer(typeof(T));
            return (T)js.ReadObject(stream);
        }

        #region file path formatter
        public static string getScenarioFilePath(string instanceName) {
            return InstanceDir + instanceName + "/Sc-" + instanceName + FileExtension;
        }

        public static string getWeekdataFilePath(string instanceName, char index) {
            return InstanceDir + instanceName + "/WD-" + instanceName + "-" + index + FileExtension;
        }

        public static string getInitHistoryFilePath(string instanceName, char index) {
            return InstanceDir + instanceName + "/H0-" + instanceName + "-" + index + FileExtension;
        }

        public static string getHistoryFilePath(char index) {
            return OutputDir + "/history-week-" + index + FileExtension;
        }
        #endregion file path formatter
        #endregion Method

        #region Property
        #endregion Property

        #region Type
        [DataContract]
        public class ScenarioInfo
        {
            [DataContract]
            public class ShiftType
            {
                [DataMember]
                public string id;
                [DataMember]
                public int minimumNumberOfConsecutiveAssignments;
                [DataMember]
                public int maximumNumberOfConsecutiveAssignments;
            }

            [DataContract]
            public class ForbiddenShiftTypeSuccession
            {
                [DataMember]
                public string precedingShiftType;
                [DataMember]
                public string[] succeedingShiftTypes;
            }

            [DataContract]
            public class Contract
            {
                [DataMember]
                public string id;
                [DataMember]
                public int minimumNumberOfAssignments;
                [DataMember]
                public int maximumNumberOfAssignments;
                [DataMember]
                public int minimumNumberOfConsecutiveWorkingDays;
                [DataMember]
                public int maximumNumberOfConsecutiveWorkingDays;
                [DataMember]
                public int minimumNumberOfConsecutiveDaysOff;
                [DataMember]
                public int maximumNumberOfConsecutiveDaysOff;
                [DataMember]
                public int maximumNumberOfWorkingWeekends;
                [DataMember]
                public int completeWeekends;
            };

            [DataContract]
            public class Nurse
            {
                [DataMember]
                public string id;
                [DataMember]
                public string contract;
                [DataMember]
                public string[] skills;
            };


            [DataMember]
            public string id;
            [DataMember]
            public int numberOfWeeks;
            [DataMember]
            public string[] skills;
            [DataMember]
            public ShiftType[] shiftTypes;
            [DataMember]
            public ForbiddenShiftTypeSuccession[] forbiddenShiftTypeSuccessions;
            [DataMember]
            public Contract[] contracts;
            [DataMember]
            public Nurse[] nurses;
        }

        [DataContract]
        public class WeekdataInfo
        {
            [DataContract]
            public class Requirement
            {
                [DataContract]
                public class DailyRequirement
                {
                    [DataMember]
                    public int minimum;
                    [DataMember]
                    public int optimal;
                }


                [DataMember]
                public string shiftType;
                [DataMember]
                public string skill;
                [DataMember]
                public DailyRequirement requirementOnMonday;
                [DataMember]
                public DailyRequirement requirementOnTuesday;
                [DataMember]
                public DailyRequirement requirementOnWednesday;
                [DataMember]
                public DailyRequirement requirementOnThursday;
                [DataMember]
                public DailyRequirement requirementOnFriday;
                [DataMember]
                public DailyRequirement requirementOnSaturday;
                [DataMember]
                public DailyRequirement requirementOnSunday;
            }

            [DataContract]
            public class ShiftOffRequest
            {
                [DataMember]
                public string nurse;
                [DataMember]
                public string shiftType;
                [DataMember]
                public string day;
            }


            [DataMember]
            public string scenario;
            [DataMember]
            public Requirement[] requirements;
            [DataMember]
            public ShiftOffRequest[] shiftOffRequests;
        }

        [DataContract]
        public class HistoryInfo
        {
            [DataContract]
            public class NurseHistoryInfo
            {
                [DataMember]
                public string nurse;
                [DataMember]
                public int numberOfAssignments;
                [DataMember]
                public int numberOfWorkingWeekends;
                [DataMember]
                public string lastAssignedShiftType;
                [DataMember]
                public int numberOfConsecutiveAssignments;
                [DataMember]
                public int numberOfConsecutiveWorkingDays;
                [DataMember]
                public int numberOfConsecutiveDaysOff;
            }


            [DataMember]
            public int week;
            [DataMember]
            public string scenario;
            [DataMember]
            public NurseHistoryInfo[] nurseHistory;
        }
        #endregion Type

        #region Constant
        public enum Instance
        {
            n005w4, n012w8, n021w4,
            n030w4, n030w8,
            n040w4, n040w8,
            n050w4, n050w8,
            n060w4, n060w8,
            n080w4, n080w8,
            n100w4, n100w8,
            n120w4, n120w8
        }

        public const string FileExtension = @".json";
        public const string InstanceDir = @"../Instance/";
        public const string OutputDir = @"output";
        #endregion Constant

        #region Field
        [DataMember]
        public ScenarioInfo scenario;
        [DataMember]
        public WeekdataInfo weekdata;
        [DataMember]
        public HistoryInfo history;
        #endregion Field
    }

    /// <summary> formatted structural input data. </summary>
    [DataContract]
    public class Problem
    {
        #region Constructor
        #endregion Constructor

        #region Method
        /// <summary> scenario must be loaded first. </summary>
        public void loadScenario(Input_INRC2Json input) {
            int i;
            int length;

            scenario.totalWeekNum = input.scenario.numberOfWeeks;
            scenario.maxWeekCount = scenario.totalWeekNum - 1;
            scenario.SkillNum = input.scenario.skills.Length;
            scenario.SkillsLength = scenario.SkillNum + SkillIDs.Begin;
            scenario.ShiftNum = input.scenario.shiftTypes.Length;

            names.scenarioName = input.scenario.id;

            names.skillNames = new string[scenario.SkillsLength];
            names.skillMap = new Dictionary<string, SkillID>();
            i = 0;
            for (SkillID sk = SkillIDs.Begin; sk < scenario.SkillsLength; i++, sk++) {
                string name = input.scenario.skills[i];
                names.skillNames[sk] = name;
                names.skillMap[name] = sk;
            }

            length = input.scenario.shiftTypes.Length + ShiftIDs.Begin;
            scenario.shifts = new ScenarioInfo.Shift[length];
            scenario.shifts[ShiftIDs.None].legalNextShifts = Util.CreateArray(length, true);
            names.shiftNames = new string[length];
            names.shiftMap = new Dictionary<string, ShiftID>();
            names.shiftMap[ShiftIDs.AnyStr] = ShiftIDs.Any;
            names.shiftMap[ShiftIDs.NoneStr] = ShiftIDs.None;
            i = 0;
            for (ShiftID sh = ShiftIDs.Begin; sh < names.shiftNames.Length; i++, sh++) {
                string name = input.scenario.shiftTypes[i].id;
                names.shiftNames[sh] = name;
                names.shiftMap[name] = sh;
                scenario.shifts[sh].minConsecutiveShiftNum =
                    input.scenario.shiftTypes[i].minimumNumberOfConsecutiveAssignments;
                scenario.shifts[sh].maxConsecutiveShiftNum =
                    input.scenario.shiftTypes[i].maximumNumberOfConsecutiveAssignments;
                scenario.shifts[sh].legalNextShifts = Util.CreateArray(length, true);
            }
            for (i = 0; i < input.scenario.forbiddenShiftTypeSuccessions.Length; i++) {
                Input_INRC2Json.ScenarioInfo.ForbiddenShiftTypeSuccession succession = input.scenario.forbiddenShiftTypeSuccessions[i];
                ShiftID priorShift = names.shiftMap[succession.precedingShiftType];
                bool[] legalNextShift = scenario.shifts[priorShift].legalNextShifts;
                for (int j = 0; j < succession.succeedingShiftTypes.Length; j++) {
                    ShiftID nextShift = names.shiftMap[succession.succeedingShiftTypes[j]];
                    scenario.shifts[priorShift].legalNextShifts[nextShift] = false;
                }
            }

            length = input.scenario.contracts.Length + ContractIDs.Begin;
            scenario.contracts = new ScenarioInfo.Contract[length];
            names.contractNames = new string[length];
            names.contractMap = new Dictionary<string, ContractID>();
            i = 0;
            for (ContractID c = ContractIDs.Begin; c < names.contractNames.Length; i++, c++) {
                string name = input.scenario.contracts[i].id;
                names.contractNames[c] = name;
                names.contractMap[name] = c;
                Input_INRC2Json.ScenarioInfo.Contract inContract = input.scenario.contracts[i];
                scenario.contracts[c].minShiftNum = inContract.minimumNumberOfAssignments;
                scenario.contracts[c].maxShiftNum = inContract.maximumNumberOfAssignments;
                scenario.contracts[c].maxWorkingWeekendNum = inContract.maximumNumberOfWorkingWeekends;
                scenario.contracts[c].completeWeekend = Convert.ToBoolean(inContract.completeWeekends);
                scenario.contracts[c].minConsecutiveDayNum = inContract.minimumNumberOfConsecutiveWorkingDays;
                scenario.contracts[c].maxConsecutiveDayNum = inContract.maximumNumberOfConsecutiveWorkingDays;
                scenario.contracts[c].minConsecutiveDayoffNum = inContract.minimumNumberOfConsecutiveDaysOff;
                scenario.contracts[c].maxConsecutiveDayoffNum = inContract.maximumNumberOfConsecutiveDaysOff;
            }

            length = input.scenario.nurses.Length + NurseIDs.Begin;
            scenario.nurses = new ScenarioInfo.Nurse[length];
            names.nurseNames = new string[length];
            names.nurseMap = new Dictionary<string, NurseID>();
            i = 0;
            for (NurseID n = NurseIDs.Begin; n < names.nurseNames.Length; i++, n++) {
                string name = input.scenario.nurses[i].id;
                names.nurseNames[n] = name;
                names.nurseMap[name] = n;
                Input_INRC2Json.ScenarioInfo.Nurse inNurse = input.scenario.nurses[i];
                scenario.nurses[n].contract = names.contractMap[inNurse.contract];
                scenario.nurses[n].skills = new bool[scenario.SkillsLength]; // default value is false which means no such skill
                for (int j = 0; j < inNurse.skills.Length; j++) {
                    scenario.nurses[n].skills[names.skillMap[inNurse.skills[j]]] = true;
                }
            }
        }

        /// <summary> weekdata must be loaded after scenario. </summary>
        public void loadWeekdata(Input_INRC2Json input) {
            // default value is false which means no shift off request
            weekdata.shiftOffs = new bool[scenario.nurses.Length, Weekdays.Length, scenario.shifts.Length];
            foreach (Input_INRC2Json.WeekdataInfo.ShiftOffRequest request in input.weekdata.shiftOffRequests) {
                NurseID nurse = names.nurseMap[request.nurse];
                Weekday weekday = Weekdays.Map[request.day];
                ShiftID shift = names.shiftMap[request.shiftType];
                weekdata.shiftOffs[nurse, weekday, shift] = true;
            }

            weekdata.minNurseNums = new int[Weekdays.Length, scenario.shifts.Length, scenario.SkillsLength];
            weekdata.optNurseNums = new int[Weekdays.Length, scenario.shifts.Length, scenario.SkillsLength];
            foreach (Input_INRC2Json.WeekdataInfo.Requirement require in input.weekdata.requirements) {
                ShiftID shift = names.shiftMap[require.shiftType];
                SkillID skill = names.skillMap[require.skill];
                weekdata.minNurseNums[Weekdays.Mon, shift, skill] = require.requirementOnMonday.minimum;
                weekdata.minNurseNums[Weekdays.Tue, shift, skill] = require.requirementOnTuesday.minimum;
                weekdata.minNurseNums[Weekdays.Wed, shift, skill] = require.requirementOnWednesday.minimum;
                weekdata.minNurseNums[Weekdays.Thu, shift, skill] = require.requirementOnThursday.minimum;
                weekdata.minNurseNums[Weekdays.Fri, shift, skill] = require.requirementOnFriday.minimum;
                weekdata.minNurseNums[Weekdays.Sat, shift, skill] = require.requirementOnSaturday.minimum;
                weekdata.minNurseNums[Weekdays.Sun, shift, skill] = require.requirementOnSunday.minimum;
                weekdata.optNurseNums[Weekdays.Mon, shift, skill] = require.requirementOnMonday.optimal;
                weekdata.optNurseNums[Weekdays.Tue, shift, skill] = require.requirementOnTuesday.optimal;
                weekdata.optNurseNums[Weekdays.Wed, shift, skill] = require.requirementOnWednesday.optimal;
                weekdata.optNurseNums[Weekdays.Thu, shift, skill] = require.requirementOnThursday.optimal;
                weekdata.optNurseNums[Weekdays.Fri, shift, skill] = require.requirementOnFriday.optimal;
                weekdata.optNurseNums[Weekdays.Sat, shift, skill] = require.requirementOnSaturday.optimal;
                weekdata.optNurseNums[Weekdays.Sun, shift, skill] = require.requirementOnSunday.optimal;
            }
        }

        /// <summary> history must be loaded after scenario. </summary>
        public void loadHistory(Input_INRC2Json input) {
            history.accObjValue = 0;
            history.pastWeekCount = input.history.week;
            history.currentWeek = history.pastWeekCount + 1;
            history.restWeekCount = scenario.totalWeekNum - history.pastWeekCount;

            history.totalAssignNums = new int[scenario.nurses.Length];
            history.totalWorkingWeekendNums = new int[scenario.nurses.Length];
            history.lastShifts = new ShiftID[scenario.nurses.Length];
            history.consecutiveShiftNums = new int[scenario.nurses.Length];
            history.consecutiveDayNums = new int[scenario.nurses.Length];
            history.consecutiveDayoffNums = new int[scenario.nurses.Length];
            foreach (Input_INRC2Json.HistoryInfo.NurseHistoryInfo h in input.history.nurseHistory) {
                NurseID nurse = names.nurseMap[h.nurse];
                history.totalAssignNums[nurse] = h.numberOfAssignments;
                history.totalWorkingWeekendNums[nurse] = h.numberOfWorkingWeekends;
                history.lastShifts[nurse] = names.shiftMap[h.lastAssignedShiftType];
                history.consecutiveShiftNums[nurse] = h.numberOfConsecutiveAssignments;
                history.consecutiveDayNums[nurse] = h.numberOfConsecutiveWorkingDays;
                history.consecutiveDayoffNums[nurse] = h.numberOfConsecutiveDaysOff;
            }
        }

        public void readCustomFile() {

        }

        public void writeCustomFile() {

        }

        public void writeSolution() {

        }

        /// <summary>
        ///	return true if two nurses have same skill. <para />
        /// the result is calculated from input data.
        /// </summary>
        public bool haveSameSkill(NurseID nurse, NurseID nurse2) {
            for (SkillID sk = ShiftIDs.Begin; sk < scenario.SkillsLength; sk++) {
                if (scenario.nurses[nurse].skills[sk] && scenario.nurses[nurse2].skills[sk]) {
                    return true;
                }
            }
            return false;
        }
        #endregion Method

        #region Property
        public ScenarioInfo Scenario { get { return scenario; } }
        public WeekdataInfo Weekdata { get { return weekdata; } }
        public HistoryInfo History { get { return history; } }
        public NameInfo Names { get { return names; } }
        #endregion Property

        #region Type
        [DataContract]
        public struct ScenarioInfo
        {
            // TUNEUP[1]: make it class? (easier for variable alias)
            [DataContract]
            public struct Shift
            {
                [DataMember]
                public int minConsecutiveShiftNum;
                [DataMember]
                public int maxConsecutiveShiftNum;
                /// <summary>
                /// (legalNextShift[nextShift] == true) means nextShift
                /// is available to be succession of this shift.
                /// </summary>
                [DataMember]
                public bool[] legalNextShifts;
            }

            [DataContract]
            public struct Contract
            {
                [DataMember]
                public int minShiftNum;         // total assignments in the planning horizon
                [DataMember]
                public int maxShiftNum;         // total assignments in the planning horizon
                [DataMember]
                public int maxWorkingWeekendNum;// total assignments in the planning horizon
                [DataMember]
                public bool completeWeekend;
                [DataMember]
                public int minConsecutiveDayNum;
                [DataMember]
                public int maxConsecutiveDayNum;
                [DataMember]
                public int minConsecutiveDayoffNum;
                [DataMember]
                public int maxConsecutiveDayoffNum;

                /// <summary> nurses with this contract. </summary>
                [DataMember]
                public NurseID[] nurses;
            };

            [DataContract]
            public struct Nurse
            {
                [DataMember]
                public ContractID contract;
                [DataMember]
                public int skillNum;
                /// <summary> (skills[skill] == true) means the nurse have that skill. </summary>
                [DataMember]
                public bool[] skills;
            };


            /// <summary> count from 1 (maxWeekCount = (totalWeekNum - 1)). </summary>
            [DataMember]
            public int totalWeekNum;
            /// <summary> count from 0. </summary>
            [DataMember]
            public int maxWeekCount;

            [DataMember]
            /// <summary> actual type number. </summary>
            public int SkillNum { get; set; }
            /// <summary> type number including sentinels. </summary>
            public int SkillsLength { get; set; }

            /// <summary> actual type number. </summary>
            [DataMember]
            public int ShiftNum { get; set; }
            /// <summary>
            ///	there may be some sentinels in shifts. <para />
            /// shifts.Length is type number including sentinels.
            /// </summary>
            [DataMember]
            public Shift[] shifts;

            public int ContractNum { get { return contracts.Length; } }
            [DataMember]
            public Contract[] contracts;

            public int NurseNum { get { return nurses.Length; } }
            [DataMember]
            public Nurse[] nurses;
        }

        [DataContract]
        public struct WeekdataInfo
        {
            /// <summary> (shiftOffs[nurse,day,shift] == true) means shiftOff required. </summary>
            [DataMember]
            public bool[, ,] shiftOffs;

            /// <summary> minNurseNums[day,shift,skill] is a number of nurse. </summary>
            [DataMember]
            public int[, ,] minNurseNums;

            /// <summary> optNurseNums[day,shift,skill] is a number of nurse. </summary>
            [DataMember]
            public int[, ,] optNurseNums;
        }

        [DataContract]
        public struct HistoryInfo
        {
            /// <summary> accumulated objective value. </summary>
            [DataMember]
            public ObjValue accObjValue;
            /// <summary> count from 0 (the number in history file). </summary>
            [DataMember]
            public int pastWeekCount;
            /// <summary> count from 1. </summary>
            [DataMember]
            public int currentWeek;
            /// <summary> include current week (restWeekCount = (totalWeekNum - pastWeekCount)). </summary>
            [DataMember]
            public int restWeekCount;

            [DataMember]
            public int[] totalAssignNums;
            [DataMember]
            public int[] totalWorkingWeekendNums;
            [DataMember]
            public ShiftID[] lastShifts;
            [DataMember]
            public int[] consecutiveShiftNums;
            [DataMember]
            public int[] consecutiveDayNums;
            [DataMember]
            public int[] consecutiveDayoffNums;
        }

        [DataContract]
        public struct NameInfo
        {
            [DataMember]
            public string scenarioName;

            /// <summary> (skillMap[skillNames[skillID]] == skillID). </summary>
            [DataMember]
            public string[] skillNames;
            /// <summary> (skillMap[skillNames[skillID]] == skillID). </summary>
            [DataMember]
            public Dictionary<string, SkillID> skillMap;

            /// <summary> (shiftMap[shiftNames[shiftID]] == shiftID). </summary>
            [DataMember]
            public string[] shiftNames;
            /// <summary> (shiftMap[shiftNames[shiftID]] == shiftID). </summary>
            [DataMember]
            public Dictionary<string, ShiftID> shiftMap;

            /// <summary> (contractMap[contractNames[contractID]] == contractID). </summary>
            [DataMember]
            public string[] contractNames;
            /// <summary> (contractMap[contractNames[contractID]] == contractID). </summary>
            [DataMember]
            public Dictionary<string, ContractID> contractMap;

            /// <summary> (nurseMap[nurseNames[nurseID]] == nurseID). </summary>
            [DataMember]
            public string[] nurseNames;
            /// <summary> (nurseMap[nurseNames[nurseID]] == nurseID). </summary>
            [DataMember]
            public Dictionary<string, NurseID> nurseMap;
        }
        #endregion Type

        #region Constant
        #endregion Constant

        #region Field
        [DataMember]
        private ScenarioInfo scenario;
        [DataMember]
        private WeekdataInfo weekdata;
        [DataMember]
        private HistoryInfo history;
        [DataMember]
        private NameInfo names;
        #endregion Field
    }
    #endregion input

    #region solver
    // TUNEUP[2]: consider class rather than struct?
    [DataContract]
    public struct Assign
    {
        #region Constructor
        public Assign(ShiftID sh, SkillID sk) {
            shift = sh;
            skill = sk;
        }
        #endregion Constructor

        #region Method
        public override bool Equals(object obj) {
            return base.Equals(obj);
        }

        public override int GetHashCode() {
            return base.GetHashCode();
        }

        public static bool operator ==(Assign left, Assign right) {
            return left.Equals(right);
        }

        public static bool operator !=(Assign left, Assign right) {
            return !left.Equals(right);
        }
        #endregion Method

        #region Property
        public bool IsWorking { get { return ShiftIDs.isWorking(shift); } }
        #endregion Property

        #region Type
        #endregion Type

        #region Constant
        #endregion Constant

        #region Field
        [DataMember]
        public ShiftID shift;
        [DataMember]
        public SkillID skill;
        #endregion Field
    }

    /// <summary> Output[nurse,day] is an Assign. </summary>
    [DataContract]
    public class Output : Util.ICopyable<Output>
    {
        #region Constructor
        /// <summary>
        /// weekdaysLength should be (ActualWeekdayNumber + 2) to let it 
        /// allocate additional days for history and next week sentinel.
        /// </summary>
        public Output(int nursesLength, int weekdaysLength, ObjValue objValue = DefaultPenalty.MaxObjValue,
            SecondaryObjValue secondaryObjValue = 0, TimePoint findTime = 0) {
            this.assignTable = new Assign[nursesLength, weekdaysLength];
            this.objValue = objValue;
            this.secondaryObjValue = secondaryObjValue;
            this.findTime = findTime;
        }

        public Output(Problem problem)
            : this(problem.Scenario.nurses.Length, Weekdays.Length_Full) {
        }

        /// <summary> mainly used for debugging. </summary>
        public Output(int nursesLength, int weekdaysLength, ObjValue objValue,
            SecondaryObjValue secondaryObjValue, TimePoint findTime, string assignStr)
            : this(nursesLength, weekdaysLength, objValue, secondaryObjValue, findTime) {
            int i = 0;
            string[] s = assignStr.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            for (NurseID nurse = NurseIDs.Begin; nurse < nursesLength; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    assignTable[nurse, weekday] =
                        new Assign(ShiftID.Parse(s[i]), SkillID.Parse(s[i + 1]));
                    i += 2;
                }
            }
        }

        public Output(Output output) {
            this.assignTable = (Assign[,])output.assignTable.Clone();
            this.objValue = output.objValue;
            this.secondaryObjValue = output.secondaryObjValue;
            this.findTime = output.findTime;
        }
        #endregion Constructor

        #region Method
        public void copyTo(Output output) {
            output.AssignTable = AssignTable;
            output.ObjValue = ObjValue;
            output.SecondaryObjValue = SecondaryObjValue;
            output.FindTime = FindTime;
        }
        #endregion Method

        #region Property
        public Assign this[NurseID nurse, Weekday weekday] {
            get { return assignTable[nurse, weekday]; }
            protected set { assignTable[nurse, weekday] = value; }
        }

        /// <summary>
        ///	protected getter to avoid outside modification on array elements. <para />
        /// use indexer to access single elements. AssignTable[nurse,day] is an Assign.
        /// </summary>
        protected Assign[,] AssignTable {
            get { return assignTable; }
            set { Array.Copy(value, assignTable, assignTable.Length); }
        }

        public ObjValue ObjValue {
            get { return objValue; }
            protected set { objValue = value; }
        }

        public SecondaryObjValue SecondaryObjValue {
            get { return secondaryObjValue; }
            protected set { secondaryObjValue = value; }
        }

        /// <summary> time from the solver start in ticks. </summary>
        public TimePoint FindTime {
            get { return findTime; }
            protected set { findTime = value; }
        }
        #endregion Property

        #region Type
        #endregion Type

        #region Constant
        #endregion Constant

        #region Field
        [DataMember]
        private Assign[,] assignTable;
        [DataMember]
        private ObjValue objValue;
        [DataMember]
        private SecondaryObjValue secondaryObjValue;
        [DataMember]
        private TimePoint findTime;
        #endregion Field
    }

    /// <summary> a solver must have a specific config derived from it. </summary>
    public abstract class SolverConfigure
    {
        #region Constructor
        /// <summary> create with default settings. </summary>
        public SolverConfigure(int id, string instanceName, int randSeed, double timeoutInSeconds) {
            // TODO[0]: update default configuration to optima.
            this.id = id;
            this.instanceName = instanceName;
            this.randSeed = randSeed;
            this.timeoutInSeconds = timeoutInSeconds;
            this.maxIterCount = IterCounts.Max;
            this.initAlgorithm = InitAlgorithm.Random;
            this.solveAlgorithm = SolveAlgorithm.BiasTabuSearch;
        }
        #endregion Constructor

        #region Method
        #endregion Method

        #region Property
        // UNDONE[2]: config name for log.
        public string Name {
            get {
                return null;
            }
        }
        #endregion Property

        #region Type
        #endregion Type

        #region Constant
        // TODO[9]: new algorithm should register to it
        public enum InitAlgorithm
        {
            Random, Greedy, Exact, Length
        };

        // TODO[9]: new algorithm should register to it
        public enum SolveAlgorithm
        {
            RandomWalk, IterativeLocalSearch, TabuSearch, BiasTabuSearch, Length
        };
        #endregion Constant

        #region Field
        [DataMember]
        public int id;

        [DataMember]
        public string instanceName;

        [DataMember]
        public int randSeed;

        [DataMember]
        public double timeoutInSeconds;
        [DataMember]
        public IterCount maxIterCount;

        [DataMember]
        public InitAlgorithm initAlgorithm;
        [DataMember]
        public SolveAlgorithm solveAlgorithm;
        #endregion Field
    }

    public abstract class Solver<TConfig> where TConfig : SolverConfigure
    {
        #region Constructor
        public Solver(Problem problem, TConfig config) {
            this.problem = problem;
            this.Config = config;
            this.rand = new Random(config.randSeed);
            this.clock = new Stopwatch();
            this.iterationCount = 0;
            this.generationCount = 0;
            this.optima = new Output(problem);
            this.endTimeInTicks = (TimePoint)((config.timeoutInSeconds - SaveSolutionTimeInSeconds) * Durations.Frequency);
            this.endTimeInMilliseconds = (TimePoint)((config.timeoutInSeconds - SaveSolutionTimeInSeconds) * Durations.MillisecondsInSecond);
        }
        #endregion Constructor

        #region Method
        /// <summary> search for optima. </summary>        
        /// <remarks> return before timeout in config is reached. </remarks>
        public abstract void solve();

        #region checkers
        /// <summary> return true if the optima solution is feasible and objValue is the same. </summary>
        public bool check() {
            bool feasible = (checkFeasibility() == 0);
            bool objValMatch = (checkObjValue() == Optima.ObjValue);

            if (!feasible) {
                errorLog("infeasible optima solution.");
            }
            if (!objValMatch) {
                errorLog("obj value does not match in optima solution.");
            }

            return (feasible && objValMatch);
        }

        /// <summary>
        /// use original input instead of auxiliary data structure
        /// return 0 if no violation of hard constraints.
        /// </summary>
        public ObjValue checkFeasibility(Output output) {
            ObjValue objValue = 0;
            countNurseNums(output);

            // check H1: Single assignment per day
            // always true

            // check H2: Under-staffing
            for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (ShiftID shift = ShiftIDs.Begin; shift < S.shifts.Length; shift++) {
                    for (SkillID skill = SkillIDs.Begin; skill < S.SkillsLength; skill++) {
                        if (assignedNurseNums[weekday, shift, skill] < W.minNurseNums[weekday, shift, skill]) {
                            objValue += DefaultPenalty.Understaff_Repair
                                * (W.minNurseNums[weekday, shift, skill] - assignedNurseNums[weekday, shift, skill]);
                        }
                    }
                }
            }

            // check H3: Shift type successions
            for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (NurseID nurse = 0; nurse < S.nurses.Length; nurse++) {
                    if (!S.shifts[output[nurse, weekday - 1].shift].legalNextShifts[output[nurse, weekday].shift]) {
                        objValue += DefaultPenalty.Succession_Repair;
                    }
                }
            }

            // check H4: Missing required skill
            for (NurseID nurse = 0; nurse < S.nurses.Length; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    if (!S.nurses[nurse].skills[output[nurse, weekday].skill]) {
                        objValue += DefaultPenalty.MissSkill_Repair;
                    }
                }
            }

            return objValue;
        }

        /// <summary> check feasibility on optima. </summary>
        public ObjValue checkFeasibility() {
            return checkFeasibility(Optima);
        }

        /// <summary> return objective value if solution is legal. </summary>
        public ObjValue checkObjValue(Output output) {
            ObjValue objValue = 0;
            countNurseNums(output);

            // check S1: Insufficient staffing for optimal coverage (30)
            for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (ShiftID shift = ShiftIDs.Begin; shift < S.shifts.Length; shift++) {
                    for (SkillID skill = SkillIDs.Begin; skill < S.SkillsLength; skill++) {
                        int missingNurse = (W.optNurseNums[weekday, shift, skill]
                            - assignedNurseNums[weekday, shift, skill]);
                        if (missingNurse > 0) {
                            objValue += DefaultPenalty.InsufficientStaff * missingNurse;
                        }
                    }
                }
            }

            // check S2: Consecutive assignments (15/30)
            // check S3: Consecutive days off (30)
            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                int consecutiveShift = H.consecutiveShiftNums[nurse];
                int consecutiveDay = H.consecutiveDayNums[nurse];
                int consecutiveDayOff = H.consecutiveDayoffNums[nurse];
                bool shiftBegin = (consecutiveShift != 0);
                bool dayBegin = (consecutiveDay != 0);
                bool dayoffBegin = (consecutiveDayOff != 0);

                checkConsecutiveViolation(ref objValue, output, nurse, Weekdays.Mon, H.lastShifts[nurse],
                    ref consecutiveShift, ref consecutiveDay, ref consecutiveDayOff,
                    ref shiftBegin, ref dayBegin, ref dayoffBegin);

                for (Weekday weekday = Weekdays.Tue; weekday <= Weekdays.Sun; weekday++) {
                    checkConsecutiveViolation(ref objValue, output, nurse, weekday, output[nurse, weekday - 1].shift,
                        ref consecutiveShift, ref consecutiveDay, ref consecutiveDayOff,
                        ref shiftBegin, ref dayBegin, ref dayoffBegin);
                }
                // since penalty was calculated when switching assign, the penalty of last 
                // consecutive assignments are not considered. so finish it here.
                ContractID cid = S.nurses[nurse].contract;
                Problem.ScenarioInfo.Contract[] cs = S.contracts;
                ShiftID shid = output[nurse, Weekdays.Sun].shift;
                Problem.ScenarioInfo.Shift[] sh = S.shifts;
                if (dayoffBegin && H.consecutiveDayoffNums[nurse] > cs[cid].maxConsecutiveDayoffNum) {
                    objValue += DefaultPenalty.ConsecutiveDayOff * Weekdays.Num;
                } else if (consecutiveDayOff > cs[cid].maxConsecutiveDayoffNum) {
                    objValue += DefaultPenalty.ConsecutiveDayOff
                        * (consecutiveDayOff - cs[cid].maxConsecutiveDayoffNum);
                } else if (consecutiveDayOff == 0) {    // working day
                    if (shiftBegin && H.consecutiveShiftNums[nurse] > sh[shid].maxConsecutiveShiftNum) {
                        objValue += DefaultPenalty.ConsecutiveShift * Weekdays.Num;
                    } else if (consecutiveShift > sh[shid].maxConsecutiveShiftNum) {
                        objValue += DefaultPenalty.ConsecutiveShift
                            * (consecutiveShift - sh[shid].maxConsecutiveShiftNum);
                    }
                    if (dayBegin && H.consecutiveDayNums[nurse] > cs[cid].maxConsecutiveDayNum) {
                        objValue += DefaultPenalty.ConsecutiveDay * Weekdays.Num;
                    } else if (consecutiveDay > cs[cid].maxConsecutiveDayNum) {
                        objValue += DefaultPenalty.ConsecutiveDay
                            * (consecutiveDay - cs[cid].maxConsecutiveDayNum);
                    }
                }
            }

            // check S4: Preferences (10)
            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    if (W.shiftOffs[weekday, output[nurse, weekday].shift, nurse]) {
                        objValue += DefaultPenalty.Preference;
                    }
                }
            }

            // check S5: Complete weekend (30)
            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                if (S.contracts[S.nurses[nurse].contract].completeWeekend
                    && (output[nurse, Weekdays.Sat].IsWorking != output[nurse, Weekdays.Sun].IsWorking)) {
                    objValue += DefaultPenalty.CompleteWeekend;
                }
            }

            // check S6: Total assignments (20)
            // check S7: Total working weekends (30)
            if (H.currentWeek == S.totalWeekNum) {
                for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                    int assignNum = H.totalAssignNums[nurse];
                    for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                        if (output[nurse, weekday].IsWorking) { assignNum++; }
                    }

                    int min = S.contracts[S.nurses[nurse].contract].minShiftNum;
                    int max = S.contracts[S.nurses[nurse].contract].maxShiftNum;
                    objValue += DefaultPenalty.TotalAssign * Util.distanceToRange(assignNum, min, max);

                    int maxWeekend = S.contracts[S.nurses[nurse].contract].maxWorkingWeekendNum;
                    int weekendNum = H.totalWorkingWeekendNums[nurse];
                    if (output[nurse, Weekdays.Sat].IsWorking || output[nurse, Weekdays.Sun].IsWorking) {
                        weekendNum++;
                    }
                    objValue += DefaultPenalty.TotalWorkingWeekend * Util.exceedCount(weekendNum, maxWeekend) / S.totalWeekNum;
                }
            }

            return objValue;
        }

        /// <summary> check objective value on optima. </summary>
        public ObjValue checkObjValue() {
            return checkObjValue(Optima);
        }

        private void checkConsecutiveViolation(ref ObjValue objValue,
            Output output, NurseID nurse, Weekday weekday, ShiftID lastShiftID,
            ref int consecutiveShift, ref int consecutiveDay, ref int consecutiveDayOff,
            ref bool shiftBegin, ref bool dayBegin, ref bool dayoffBegin) {
            ContractID cid = S.nurses[nurse].contract;
            Problem.ScenarioInfo.Contract[] cs = S.contracts;
            Problem.ScenarioInfo.Shift[] sh = S.shifts;
            ShiftID shift = output[nurse, weekday].shift;
            if (ShiftIDs.isWorking(shift)) {    // working day
                if (consecutiveDay == 0) {  // switch from consecutive day off to working
                    if (dayoffBegin) {
                        if (H.consecutiveDayoffNums[nurse] > cs[cid].maxConsecutiveDayoffNum) {
                            objValue += DefaultPenalty.ConsecutiveDayOff * (weekday - Weekdays.Mon);
                        } else {
                            objValue += DefaultPenalty.ConsecutiveDayOff * Util.distanceToRange(consecutiveDayOff,
                                cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        }
                        dayoffBegin = false;
                    } else {
                        objValue += DefaultPenalty.ConsecutiveDayOff * Util.distanceToRange(consecutiveDayOff,
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                    }
                    consecutiveDayOff = 0;
                    consecutiveShift = 1;
                } else {    // keep working
                    if (shift == lastShiftID) {
                        consecutiveShift++;
                    } else { // another shift
                        if (shiftBegin) {
                            if (H.consecutiveShiftNums[nurse] > sh[lastShiftID].maxConsecutiveShiftNum) {
                                objValue += DefaultPenalty.ConsecutiveShift * (weekday - Weekdays.Mon);
                            } else {
                                objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                                    sh[lastShiftID].minConsecutiveShiftNum, sh[lastShiftID].maxConsecutiveShiftNum);
                            }
                            shiftBegin = false;
                        } else {
                            objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                                sh[lastShiftID].minConsecutiveShiftNum, sh[lastShiftID].maxConsecutiveShiftNum);
                        }
                        consecutiveShift = 1;
                    }
                }
                consecutiveDay++;
            } else {    // day off
                if (consecutiveDayOff == 0) {   // switch from consecutive working to day off
                    if (shiftBegin) {
                        if (H.consecutiveShiftNums[nurse] > sh[lastShiftID].maxConsecutiveShiftNum) {
                            objValue += DefaultPenalty.ConsecutiveShift * (weekday - Weekdays.Mon);
                        } else {
                            objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                                sh[lastShiftID].minConsecutiveShiftNum, sh[lastShiftID].maxConsecutiveShiftNum);
                        }
                        shiftBegin = false;
                    } else {
                        objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                            sh[lastShiftID].minConsecutiveShiftNum, sh[lastShiftID].maxConsecutiveShiftNum);
                    }
                    if (dayBegin) {
                        if (H.consecutiveDayNums[nurse] > cs[cid].maxConsecutiveDayNum) {
                            objValue += DefaultPenalty.ConsecutiveDay * (weekday - Weekdays.Mon);
                        } else {
                            objValue += DefaultPenalty.ConsecutiveDay * Util.distanceToRange(consecutiveDay,
                                cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        }
                        dayBegin = false;
                    } else {
                        objValue += DefaultPenalty.ConsecutiveDay * Util.distanceToRange(consecutiveDay,
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                    }
                    consecutiveShift = 0;
                    consecutiveDay = 0;
                }
                consecutiveDayOff++;
            }
        }

        /// <summary> generate nurse numbers in $assignedNurseNums[day,shift,skill]. </summary>
        private void countNurseNums(Output output) {
            if (assignedNurseNums == null) {
                assignedNurseNums = new int[Weekdays.Length,
                    S.shifts.Length, S.SkillsLength];
            }

            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    Assign a = output[nurse, weekday];
                    assignedNurseNums[weekday, a.shift, a.skill]++;
                }
            }
        }
        #endregion checkers

        /// <summary> print simple information of the solution to console. </summary>
        public void print() {
            Console.WriteLine("optima.objVal: " + (optima.ObjValue / (double)DefaultPenalty.Amp));
        }

        /// <summary> record optima to specified log file. </summary>
        public void record(string logFileName) {
            if (!File.Exists(logFileName)) {
                File.WriteAllText(logFileName,
                    "Time,ID,Instance,Config,RandSeed,GenCount,IterCount,Duration,Feasible,Check-Obj,ObjValue,AccObjValue,Solution");
            }

            StringBuilder log = new StringBuilder(Environment.NewLine);
            log.Append(DateTime.Now.ToString(Durations.TimeFormat_Readable)).Append(",")
                .Append(Config.id).Append(",")
                .Append(Config.instanceName).Append(",")
                .Append(Config.Name).Append(",")
                .Append(Config.randSeed).Append(",")
                .Append(generationCount).Append(",")
                .Append(iterationCount).Append(",")
                .Append(Optima.FindTime / Durations.Frequency).Append("s,")
                .Append(checkFeasibility()).Append(",")
                .Append((checkObjValue() - Optima.ObjValue) / DefaultPenalty.Amp).Append(",")
                .Append(Optima.ObjValue / DefaultPenalty.Amp).Append(",")
                .Append((Optima.ObjValue + H.accObjValue) / DefaultPenalty.Amp).Append(",");

            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    Assign assign = Optima[nurse, weekday];
                    log.Append(assign.shift).Append(" ").Append(assign.skill).Append(" ");
                }
            }

            File.AppendAllText(logFileName, log.ToString());
        }

        /// <summary> log to console with time and runID. </summary>
        [Conditional("INRC2_LOG")]
        public void errorLog(string msg) {
            Console.Error.Write(DateTime.Now.ToString(Durations.TimeFormat_Readable));
            Console.Error.Write("," + Config.id + ",");
            Console.Error.WriteLine(msg);
        }

        /// <summary> generate history for next week from optima. </summary>
        public abstract void generateHistory(out Problem.HistoryInfo newHistory);

        /// <summary> return true if global optima or population is updated. </summary>
        protected abstract bool updateOptima(Output localOptima);
        #endregion Method

        #region Property
        public Output Optima {
            get { return optima; }
            protected set { value.copyTo(optima); }
        }

        public bool IsTimeout { get { return (clock.ElapsedTicks > endTimeInTicks); } }
        public Duration TimeLeft { get { return (Duration)(endTimeInMilliseconds - clock.ElapsedMilliseconds); } }

        protected abstract TConfig Config { get; set; }

        /// <summary> short hand for scenario. </summary>
        protected Problem.ScenarioInfo S { get { return problem.Scenario; } }
        /// <summary> short hand for weekdata. </summary>
        protected Problem.WeekdataInfo W { get { return problem.Weekdata; } }
        /// <summary> short hand for history. </summary>
        protected Problem.HistoryInfo H { get { return problem.History; } }
        #endregion Property

        #region Type
        protected class PenaltyMode
        {
            #region hard constraints
            public ObjValue singleAssign;
            public ObjValue understaff;
            public ObjValue succession;
            public ObjValue missSkill;
            #endregion hard constraints

            #region soft constraints
            public ObjValue insufficientStaff;
            public ObjValue consecutiveShift;
            public ObjValue consecutiveDay;
            public ObjValue consecutiveDayOff;
            public ObjValue preference;
            public ObjValue completeWeekend;
            public ObjValue totalAssign;
            public ObjValue totalWorkingWeekend;
            #endregion soft constraints
        }

        protected class Penalty
        {
            #region Constructor
            public Penalty() {
                modeStack = new Stack<PenaltyMode>();
                reset();
            }
            #endregion Constructor

            #region Method
            // reset to default penalty mode and clear mode stack
            public void reset() {
                modeStack.Clear();

                pm.singleAssign = DefaultPenalty.ForbiddenMove;
                pm.understaff = DefaultPenalty.ForbiddenMove;
                pm.succession = DefaultPenalty.ForbiddenMove;
                pm.missSkill = DefaultPenalty.ForbiddenMove;

                pm.insufficientStaff = DefaultPenalty.InsufficientStaff;
                pm.consecutiveShift = DefaultPenalty.ConsecutiveShift;
                pm.consecutiveDay = DefaultPenalty.ConsecutiveDay;
                pm.consecutiveDayOff = DefaultPenalty.ConsecutiveDayOff;
                pm.preference = DefaultPenalty.Preference;
                pm.completeWeekend = DefaultPenalty.CompleteWeekend;
                pm.totalAssign = DefaultPenalty.TotalAssign;
                pm.totalWorkingWeekend = DefaultPenalty.TotalWorkingWeekend;
            }

            public void recoverLastMode() {
                pm = modeStack.Pop();
            }

            public void setSwapMode() {
                modeStack.Push(pm);

                pm.understaff = 0;          // due to no extra assignments
                pm.insufficientStaff = 0;   // due to no extra assignments
            }

            public void setBlockSwapMode() {
                modeStack.Push(pm);

                pm.understaff = 0;  // due to no extra assignments
                pm.succession = 0;  // due to it is checked manually
                pm.missSkill = 0;   // due to it is checked manually
                pm.insufficientStaff = 0;   // due to no extra assignments
            }

            public void setExchangeMode() {
                modeStack.Push(pm);

                pm.succession = 0;  // due to it is checked manually
                pm.missSkill = 0;   // due to it is the same nurse
                pm.totalAssign = 0; // due to it is the same nurse
            }

            /// <summary>
            /// allow hard constraints Understaff and Succession being violated, <para />
            /// but with much greater penalty than soft constraints. <para />
            /// set softConstraintDecay to MaxObjValue to make them does not count.
            /// </summary>
            public void setRepairMode(ObjValue WeightOnUnderStaff = DefaultPenalty.Understaff_Repair,
                ObjValue WeightOnSuccesion = DefaultPenalty.Succession_Repair,
                ObjValue softConstraintDecay = DefaultPenalty.MaxObjValue) {
                modeStack.Push(pm);

                pm.understaff = WeightOnUnderStaff;
                pm.succession = WeightOnSuccesion;
                pm.insufficientStaff = DefaultPenalty.InsufficientStaff / softConstraintDecay;
                pm.consecutiveShift = DefaultPenalty.ConsecutiveShift / softConstraintDecay;
                pm.consecutiveDay = DefaultPenalty.ConsecutiveDay / softConstraintDecay;
                pm.consecutiveDayOff = DefaultPenalty.ConsecutiveDayOff / softConstraintDecay;
                pm.preference = DefaultPenalty.Preference / softConstraintDecay;
                pm.completeWeekend = DefaultPenalty.CompleteWeekend / softConstraintDecay;
                pm.totalAssign = DefaultPenalty.TotalAssign / softConstraintDecay;
                pm.totalWorkingWeekend = DefaultPenalty.TotalWorkingWeekend / softConstraintDecay;
            }
            #endregion Method

            #region Property
            #region hard constraint
            public ObjValue Understaff { get { return pm.understaff; } }
            public ObjValue SingleAssign { get { return pm.singleAssign; } }
            public ObjValue Succession { get { return pm.succession; } }
            public ObjValue MissSkill { get { return pm.missSkill; } }
            #endregion hard constraint
            #region soft constraint
            public ObjValue InsufficientStaff { get { return pm.insufficientStaff; } }
            public ObjValue ConsecutiveShift { get { return pm.consecutiveShift; } }
            public ObjValue ConsecutiveDay { get { return pm.consecutiveDay; } }
            public ObjValue ConsecutiveDayOff { get { return pm.consecutiveDayOff; } }
            public ObjValue Preference { get { return pm.preference; } }
            public ObjValue CompleteWeekend { get { return pm.completeWeekend; } }
            public ObjValue TotalAssign { get { return pm.totalAssign; } }
            public ObjValue TotalWorkingWeekend { get { return pm.totalWorkingWeekend; } }
            #endregion soft constraint
            #endregion Property

            #region Type
            #endregion Type

            #region Constant
            #endregion Constant

            #region Field
            private PenaltyMode pm;
            private Stack<PenaltyMode> modeStack;
            #endregion Field
        }
        #endregion Type

        #region Constant
        /// <summary> preserved time for IO in the total given time. </summary>
        public const double SaveSolutionTimeInSeconds = 0.5;
        #endregion Constant

        #region Field
        public readonly Problem problem;

        protected readonly Random rand;
        protected readonly Stopwatch clock;
        protected IterCount iterationCount;
        protected IterCount generationCount;

        private Output optima;

        /// <summary> 
        /// assignedNurseNums[day,shift,skill] is nurse numbers in that slot.
        /// hold the array to avoid reallocation in countNurseNums(). 
        /// </summary>
        private int[, ,] assignedNurseNums;

        /// <summary> when clock time is greater than this, the solver should stop. </summary>
        private TimePoint endTimeInTicks;
        private TimePoint endTimeInMilliseconds;
        #endregion Field
    }

    public class TabuSolver : Solver<TabuSolver.Configure>
    {
        #region Constructor
        public TabuSolver(Problem problem, Configure config)
            : base(problem, config) {
            // TUNEUP[3]: make them static method with $this as first parameter
            //          to avoid maintaining the table for every solution object.
            generateInitSolution = new GenerateInitSolution[] {
                randomInit, greedyInit, exactInit
            };
            searchForOptima = new SearchForOptima[]{
                randomWalk, iterativeLocalSearch, tabuSearch, biasTabuSearch
            };
        }
        #endregion Constructor

        #region Method
        public override void solve() {
            init();
            searchForOptima[(int)config.solveAlgorithm]();
            // TODO[6]: handle constraints on the entire planning horizon.
        }

        public override void generateHistory(out Problem.HistoryInfo newHistory) {
            Solution s = new Solution(this);
            s.rebuild(Optima);
            s.generateHistory(out newHistory);
        }

        public bool haveSameSkill(NurseID nurse, NurseID nurse2) {
            return nursesHasSameSkill[nurse, nurse2];
        }

        protected override bool updateOptima(Output localOptima) {
            if (localOptima.ObjValue < Optima.ObjValue) {
                Optima = localOptima;
                return true;
            } else if (localOptima.ObjValue == Optima.ObjValue) {
#if INRC2_SECONDARY_OBJ_VALUE
                bool isSelected = (localOptima.SecondaryObjValue < Optima.SecondaryObjValue);
#else // INRC2_SECONDARY_OBJ_VALUE
                bool isSelected = ((rand.Next(2)) == 0);
#endif // INRC2_SECONDARY_OBJ_VALUE
                if (isSelected) {
                    Optima = localOptima;
                    return true;
                }
            }

            return false;
        }

        #region init
        /// <summary> set parameters and methods, generate initial solution. </summary>
        /// <remarks> must return before timeout in config is reached. </remarks>
        protected void init() {
            clock.Start();

            calculateRestWorkload();
            discoverNurseSkillRelation();
            countTotalOptNurseNum();

            setTabuTenure();
            setMaxNoImprove();

            sln = new Solution(this);
            generateInitSolution[(int)config.initAlgorithm]();
            Optima = sln;
        }

        /// <summary>
        /// initialize max increase with same delta each week. <para />
        /// do not count min shift number in early weeks (with macro on).
        /// </summary>
        private void calculateRestWorkload() {
            restMinShiftNum = new int[S.nurses.Length];
            restMaxShiftNum = new int[S.nurses.Length];
            restMaxWorkingWeekendNum = new int[S.nurses.Length];

            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                ContractID cid = S.nurses[nurse].contract;
                Problem.ScenarioInfo.Contract[] cs = S.contracts;
#if INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
                int weekToStartCountMin = S.totalWeekNum * cs[cid].minShiftNum;
                bool ignoreMinShift = ((H.currentWeek * cs[cid].maxShiftNum) < weekToStartCountMin);
                restMinShiftNum[nurse] = (ignoreMinShift) ? 0 : (cs[cid].minShiftNum - H.totalAssignNums[nurse]);
#else // INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
                restMinShiftNum[nurse] = cs[cid].minShiftNum - H.totalAssignNums[nurse];
#endif // INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
                restMaxShiftNum[nurse] = cs[cid].maxShiftNum - H.totalAssignNums[nurse];
                restMaxWorkingWeekendNum[nurse] = cs[cid].maxWorkingWeekendNum - H.totalWorkingWeekendNums[nurse];
            }
        }

        /// <summary> initialize assist data about nurse-skill relation. </summary>
        private void discoverNurseSkillRelation() {
            // UNDONE[1]: discoverNurseSkillRelation
            nursesHasSameSkill = new bool[S.nurses.Length, S.nurses.Length];

            for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                for (NurseID nurse2 = NurseIDs.Begin; nurse < S.nurses.Length; nurse2++) {
                    nursesHasSameSkill[nurse, nurse2] = problem.haveSameSkill(nurse, nurse2);
                }
            }
        }

        private void countTotalOptNurseNum() {
            totalOptNurseNum = 0;
            for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (ShiftID shift = ShiftIDs.Begin; shift < S.shifts.Length; shift++) {
                    for (SkillID skill = SkillIDs.Begin; skill < S.SkillsLength; skill++) {
                        totalOptNurseNum += W.optNurseNums[weekday, shift, skill];
                    }
                }
            }
        }

        private void setTabuTenure() {
            // plus 1 to make sure it will not be 0
            // table size
            DayTabuTenureBase = (IterCount)(1 + config.dayTabuFactors.tableSize
                * S.NurseNum * Weekdays.Num);
            ShiftTabuTenureBase = (IterCount)(1 + config.shiftTabuFactors.tableSize
                * S.NurseNum * Weekdays.Num * S.ShiftNum * S.SkillNum);
            // nurse number
            DayTabuTenureBase *= (IterCount)(1 + config.dayTabuFactors.nurseNum * S.NurseNum);
            ShiftTabuTenureBase *= (IterCount)(1 + config.shiftTabuFactors.nurseNum * S.NurseNum);
            // day number
            DayTabuTenureBase *= (IterCount)(1 + config.dayTabuFactors.dayNum * Weekdays.Num);
            ShiftTabuTenureBase *= (IterCount)(1 + config.shiftTabuFactors.dayNum * Weekdays.Num);
            // shift number
            DayTabuTenureBase *= (IterCount)(1 + config.dayTabuFactors.shiftNum
                * S.ShiftNum * S.SkillNum);
            ShiftTabuTenureBase *= (IterCount)(1 + config.shiftTabuFactors.shiftNum
                * S.ShiftNum * S.SkillNum);

            if (DayTabuTenureBase < config.minTabuBase) { DayTabuTenureBase = config.minTabuBase; }
            if (ShiftTabuTenureBase < config.minTabuBase) { ShiftTabuTenureBase = config.minTabuBase; }
            DayTabuTenureAmp = 1 + DayTabuTenureBase / config.inverseTabuAmpRatio;
            ShiftTabuTenureAmp = 1 + ShiftTabuTenureBase / config.inverseTabuAmpRatio;
        }

        private void setMaxNoImprove() {
            MaxNoImproveForSingleNeighborhood = (IterCount)(
                config.maxNoImproveFactor * S.NurseNum * Weekdays.Num);
            MaxNoImproveForAllNeighborhood = (IterCount)(
                config.maxNoImproveFactor * S.NurseNum * Weekdays.Num
                * Math.Sqrt(S.ShiftNum * S.SkillNum));

            MaxNoImproveForBiasTabuSearch = MaxNoImproveForSingleNeighborhood / config.inverseTotalBiasRatio;

            MaxNoImproveSwapChainLength = MaxNoImproveForSingleNeighborhood;
            MaxSwapChainRestartCount = (IterCount)(Math.Sqrt(S.NurseNum));
        }

        private void randomInit() {
            if (!Util.Worker.WorkUntilTimeout(sln.generateInitSolution_Random, (int)TimeLeft)) {
                errorLog("randomInit() fail to generate feasible init solution.");
            }
        }

        private void greedyInit() {
            int greedyInitRetryCount = (int)Math.Sqrt(S.NurseNum);
            for (; greedyInitRetryCount > 0; greedyInitRetryCount--) {
                sln.generateInitSolution_Greedy();
                if (sln.ObjValue < DefaultPenalty.MaxObjValue) { return; }
            }
            if (!Util.Worker.WorkUntilTimeout(sln.repair_AlwaysEvaluateObjValue, (int)TimeLeft)) {
                errorLog("greedyInit() fail to generate feasible init solution.");
            }
        }

        private void exactInit() {
            Duration initTime = TimeLeft - (TimeLeft / InverseRatioOfRepairTimeInInit);
            if (!Util.Worker.WorkUntilTimeout(sln.generateInitSolution_BranchAndCut, (int)initTime)) {
                if (!Util.Worker.WorkUntilTimeout(sln.repair_AlwaysEvaluateObjValue, (int)TimeLeft)) {
                    errorLog("exactInit() fail to generate feasible init solution.");
                }
            }
        }
        #endregion init

        /// <summary> turn the objective to optimize a subset of nurses when no improvement. </summary>
        private void biasTabuSearch() {

        }

        /// <summary> search with tabu table. </summary>
        private void tabuSearch() {

        }

        /// <summary> iteratively run local search and perturb. </summary>
        private void iterativeLocalSearch() {

        }

        /// <summary> random walk until timeout. </summary>
        private void randomWalk() {

        }
        #endregion Method

        #region Property
        protected override Configure Config {
            get { return config; }
            set { config = value; }
        }
        #endregion Property

        #region Type
        [DataContract]
        public class Solution : Output
        {
            #region Constructor
            public Solution(TabuSolver solver)
                : base(solver.problem) {
                // init sentinels
                for (NurseID nurse = NurseIDs.Begin; nurse < solver.S.nurses.Length; nurse++) {
                    base.AssignTable[nurse, Weekdays.LastWeek].shift = solver.problem.History.lastShifts[nurse];
                    base.AssignTable[nurse, Weekdays.NextWeek].shift = ShiftIDs.None;
                }

                this.solver = solver;
                this.problem = solver.problem;

                this.optima = new Output(this.problem);

                this.penalty = new Penalty();
                this.nurseWeights = new ObjValue[S.nurses.Length];

                this.iterCount = 1;

                this.totalAssignNums = new int[S.nurses.Length];
                this.assignedNurseNums = new int[Weekdays.Length, S.shifts.Length, S.SkillsLength];
                this.missingNurseNums = new int[Weekdays.Length, S.shifts.Length, S.SkillsLength];

                this.consecutives = new Consecutive[S.nurses.Length];
                this.consecutives_Backup = new Consecutive[S.nurses.Length];
                for (NurseID nurse = 0; nurse < S.NurseNum; nurse++) {
                    this.consecutives[nurse] = new Consecutive();
                    this.consecutives_Backup[nurse] = new Consecutive(problem, nurse);
                }

                this.blockSwapCache = new BlockSwapCacheItem[S.nurses.Length, S.nurses.Length];
                for (int i = NurseIDs.Begin; i < S.nurses.Length; i++) {
                    for (int j = NurseIDs.Begin; j < S.nurses.Length; j++) {
                        this.blockSwapCache[i, j] = new BlockSwapCacheItem();
                    }
                }
                this.isBlockSwapCacheValid = new bool[S.nurses.Length];

#if INRC2_USE_TABU
                // TUNEUP[9]: remove dependency on default value of IterCount to be 0.
                this.shiftTabu = new IterCount[S.nurses.Length, Weekdays.Length,
                    S.shifts.Length, S.SkillsLength];
                this.dayTabu = new IterCount[S.nurses.Length, Weekdays.Length];
#endif // INRC2_USE_TABU

                // TUNEUP[3]: make them static method with $this as first parameter
                //          to avoid maintaining the table for every solution object.
                findBestMove = new FindBestMove[] { 
                    this.findBestAdd,
                    this.findBestRemove,
                    this.findBestChange,
                    this.findBestExchange,
                    this.findBestSwap,
#if INRC2_BLOCK_SWAP_CACHED
                    this.findBestBlockSwap_Cached,
#elif INRC2_BLOCK_SWAP_BASE
                    this.findBestBlockSwap_Base,
#elif INRC2_BLOCK_SWAP_FAST
                    this.findBestBlockSwap_Fast,
#elif INRC2_BLOCK_SWAP_PART
                    this.findBestBlockSwap_Part,
#elif INRC2_BLOCK_SWAP_RAND
                    this.findBestBlockSwap_Rand,
#endif
                    this.findBestBlockShift,
                    this.findBestARRand,
                    this.findBestARBoth
                };
                tryMove = new TryMove[] {
                    this.tryAddAssign,
                    this.tryRemoveAssign,
                    this.tryChangeAssign,
                    this.tryExchangeDay,
                    this.trySwapNurse,
                    this.trySwapBlock 
                };
                applyMove = new ApplyMove[] { 
                    this.addAssign,
                    this.removeAssign,
                    this.changeAssign,
                    this.exchangeDay,
                    this.swapNurse,
                    this.swapBlock
                };
                updateTabu = new UpdateTabu[] {
                    this.updateAddTabu,
                    this.updateRemoveTabu,
                    this.updateChangeTabu,
                    this.updateExchangeTabu,
                    this.updateSwapTabu,
                    this.updateBlockSwapTabu 
                };
            }
            #endregion Constructor

            #region Method
            public void generateInitSolution_Random() {
                resetAssign();
                resetAssistData();

                NurseID nurse;
                Weekday weekday;
                Assign assign;

                for (int i = 0; i < solver.totalOptNurseNum; i++) {
                    nurse = solver.rand.Next(NurseIDs.Begin, S.nurses.Length);
                    weekday = solver.rand.Next(Weekdays.Mon, Weekdays.NextWeek);
                    assign.shift = solver.rand.Next(ShiftIDs.Begin, S.shifts.Length);
                    assign.skill = solver.rand.Next(SkillIDs.Begin, S.SkillsLength);

                    if ((!AssignTable[nurse, weekday].IsWorking)
                        && S.nurses[nurse].skills[assign.skill]) {
                        addAssign(weekday, nurse, assign);
                    }
                }

                repair_AlwaysEvaluateObjValue();
            }
            public void generateInitSolution_Greedy() {
                resetAssign();
                resetAssistData();

                // UNDONE[6]: generateInitSolution_Greedy
            }
            public void generateInitSolution_BranchAndCut() {
                resetAssign();

                if (fillAssign(Weekdays.Mon, ShiftIDs.Begin, SkillIDs.Begin, NurseIDs.Begin, 0)) {
                    rebuild();
                } else {
                    ObjValue = DefaultPenalty.ForbiddenMove;
                }
            }

            /// <summary> get history for next week, only used for custom file. </summary>
            public void generateHistory(out Problem.HistoryInfo newHistory) {
                newHistory = new Problem.HistoryInfo();

                newHistory.accObjValue = H.accObjValue + ObjValue;
                newHistory.pastWeekCount = H.currentWeek;
                newHistory.currentWeek = H.currentWeek + 1;
                newHistory.restWeekCount = H.restWeekCount - 1;

                newHistory.totalAssignNums = (int[])(H.totalAssignNums.Clone());
                newHistory.totalWorkingWeekendNums = (int[])(H.totalWorkingWeekendNums.Clone());
                newHistory.lastShifts = new ShiftID[S.nurses.Length];
                newHistory.consecutiveShiftNums = new int[S.nurses.Length];
                newHistory.consecutiveDayNums = new int[S.nurses.Length];
                newHistory.consecutiveDayoffNums = new int[S.nurses.Length];

                for (NurseID nurse = 0; nurse < S.nurses.Length; nurse++) {
                    newHistory.totalAssignNums[nurse] += totalAssignNums[nurse];
                    if (AssignTable[nurse, Weekdays.Sat].IsWorking
                        || AssignTable[nurse, Weekdays.Sun].IsWorking) {
                        newHistory.totalWorkingWeekendNums[nurse]++;
                    }
                    newHistory.lastShifts[nurse] = AssignTable[nurse, Weekdays.Sun].shift;
                    Consecutive c = consecutives[nurse];
                    if (AssignTable[nurse, Weekdays.Sun].IsWorking) {
                        newHistory.consecutiveShiftNums[nurse] =
                            c.shiftHigh[Weekdays.Sun] - c.shiftLow[Weekdays.Sun] + 1;
                        newHistory.consecutiveDayNums[nurse] =
                            c.dayHigh[Weekdays.Sun] - c.dayLow[Weekdays.Sun] + 1;
                    } else {
                        newHistory.consecutiveDayoffNums[nurse] =
                            c.dayHigh[Weekdays.Sun] - c.dayLow[Weekdays.Sun] + 1;
                    }
                }
            }

            /// <summary>
            /// select single neighborhood to search in each iteration randomly
            /// the random select process is a discrete distribution
            /// the possibility to be selected will increase if the neighborhood
            /// improve the solution, else decrease it. the sum of possibilities is 1.0
            /// </summary>
            public void tabuSearch_Rand(FindBestMove[] findBestMove_Search, IterCount maxNoImproveCount) {
#if INRC2_PERFORMANCE_TEST
                IterCount startIterCount = iterCount;
                Stopwatch clock = new Stopwatch();
                clock.Start();
#endif
                const int weight_Invalid = 128;     // min weight
                const int weight_NoImprove = 256;
                const int weight_ImproveCur = 1024;
                const int weight_ImproveOpt = 4096; // max weight (less than (RAND_MAX / findBestMove_Search.Length))
                const int initWeight = (weight_ImproveCur + weight_NoImprove) / 2;
                const int deltaIncRatio = 8;    // = weights[mode] / weightDelta
                const int incError = deltaIncRatio - 1;
                const int deltaDecRatio = 8;    // = weights[mode] / weightDelta
                const int decError = -(deltaDecRatio - 1);

                Optima = this;

                int[] weights = new int[findBestMove_Search.Length];
                weights.fill(initWeight);
                int totalWeight = initWeight * findBestMove_Search.Length;

                IterCount noImprove = maxNoImproveCount;
                Move bestMove = new Move();
                for (; (noImprove > 0) && (iterCount < solver.Config.maxIterCount); iterCount++) {
                    int modeSelect = 0;
                    for (int w = solver.rand.Next(totalWeight); (w -= weights[modeSelect]) >= 0; modeSelect++) { }

                    bestMove.delta = DefaultPenalty.ForbiddenMove;
                    findBestMove_Search[modeSelect](ref bestMove);

                    int weightDelta;
                    if (bestMove.delta < DefaultPenalty.MaxObjValue) {
#if INRC2_USE_TABU
                        // update tabu list first because it requires original assignment
                        updateTabu[bestMove.ModeIndex](ref bestMove);
#endif
                        applyBasicMove(ref bestMove);

                        if (updateOptima()) {   // improve optima
#if INRC2_LS_AFTER_TSR_UPDATE_OPT
                            localSearch( timer, findBestMoveTable );
#endif
                            noImprove = maxNoImproveCount;
                            weightDelta = (incError + weight_ImproveOpt - weights[modeSelect]) / deltaIncRatio;
                        } else {
                            noImprove--;
                            if (bestMove.delta < 0) {    // improve current solution
                                weightDelta = (weights[modeSelect] < weight_ImproveCur)
                                    ? (incError + weight_ImproveCur - weights[modeSelect]) / deltaIncRatio
                                    : (decError + weight_ImproveCur - weights[modeSelect]) / deltaDecRatio;
                            } else {    // no improve but valid
                                weightDelta = (weights[modeSelect] < weight_NoImprove)
                                    ? (incError + weight_NoImprove - weights[modeSelect]) / deltaIncRatio
                                    : (decError + weight_NoImprove - weights[modeSelect]) / deltaDecRatio;
                            }
                        }
                    } else {    // invalid
                        weightDelta = (decError + weight_Invalid - weights[modeSelect]) / deltaDecRatio;
                    }

                    weights[modeSelect] += weightDelta;
                    totalWeight += weightDelta;
                }
#if INRC2_PERFORMANCE_TEST
                IterCount iterDelta = (iterCount - startIterCount);
                Console.WriteLine("[tabuSearch_Rand] speed: "
                    + (iterDelta * Durations.MillisecondsInSecond / clock.ElapsedMilliseconds)
                    + " iter: " + iterDelta + " time: " + clock.ElapsedMilliseconds);
#endif
            }
            /// <summary>
            /// loop to select neighborhood to search until timeout or there is no
            /// improvement on (NeighborhoodNum + 2) neighborhood consecutively.
            /// switch neighborhood when maxNoImproveForSingleNeighborhood has
            /// been reach, then restart from optima in current trajectory.
            /// </summary>
            public void tabuSearch_Loop(FindBestMove[] findBestMove_Search, IterCount maxNoImproveCount) {
#if INRC2_PERFORMANCE_TEST
                IterCount startIterCount = iterCount;
                Stopwatch clock = new Stopwatch();
                clock.Start();
#endif
                Optima = this;

                int failCount = findBestMove_Search.Length;
                int modeSelect = 0;
                // since there is randomness on the search trajectory, there will be 
                // chance to make a difference on neighborhoods which have been searched.
                // so repeat (findBestMove_Search.Length + 1) times.
                while (failCount >= 0) {
                    // reset current solution to best solution found in last neighborhood
                    rebuild(Optima);

                    IterCount noImprove_Single = maxNoImproveCount;
                    Move bestMove = new Move();
                    for (; (noImprove_Single > 0) && (iterCount < solver.Config.maxIterCount); iterCount++) {
                        bestMove.delta = DefaultPenalty.ForbiddenMove;
                        findBestMove_Search[modeSelect](ref bestMove);

                        if (bestMove.delta >= DefaultPenalty.MaxObjValue) { break; }
#if INRC2_USE_TABU
                        // update tabu list first because it requires original assignment
                        updateTabu[bestMove.ModeIndex](ref bestMove);
#endif
                        applyBasicMove(ref bestMove);

                        if (updateOptima()) {   // improved
                            failCount = findBestMove_Search.Length;
                            noImprove_Single = maxNoImproveCount;
                        } else {    // not improved
                            noImprove_Single--;
                        }
                    }

                    failCount--;
                    modeSelect++;
                    modeSelect %= findBestMove_Search.Length;
                }
#if INRC2_PERFORMANCE_TEST
                IterCount iterDelta = (iterCount - startIterCount);
                Console.WriteLine("[tabuSearch_Loop] speed: "
                    + (iterDelta * Durations.MillisecondsInSecond / clock.ElapsedMilliseconds)
                    + " iter: " + iterDelta + " time: " + clock.ElapsedMilliseconds);
#endif
            }
            /// <summary>
            /// randomly select neighborhood to search until timeout or no improve move count 
            /// reaches maxNoImproveForAllNeighborhood. for each neighborhood i, the possibility 
            /// to select is P[i]. increase the possibility to select when no improvement. in detail, 
            /// the P[i] contains two part, local and global. the local part will increase if the 
            /// neighborhood i makes improvement, and decrease vice versa. the global part will 
            /// increase if recent search (not only on neighborhood i) can not make improvement, 
            /// otherwise, it will decrease. in case the iteration  takes too much time, it can be 
            /// changed from best improvement to first improvement. if no neighborhood has been selected, 
            /// prepare a loop queue. select the one by one in the queue until a valid move is found. 
            /// move the head to the tail of the queue if it makes no improvement.
            /// </summary>
            public void tabuSearch_Possibility(FindBestMove[] findBestMove_Search, IterCount maxNoImproveCount) {
#if INRC2_PERFORMANCE_TEST
                IterCount startIterCount = iterCount;
                Stopwatch clock = new Stopwatch();
                clock.Start();
#endif
                Optima = this;

                int modeNum = findBestMove_Search.Length;
                int startMode = 0;

                double maxP_Local = 1.0 / modeNum;
                double maxP_Global = 1 - maxP_Local;
                double amp_Local = 1.0 / (2 * modeNum);
                double amp_Global = 1.0 / (4 * modeNum * modeNum);
                double dec_Local = (2.0 * modeNum - 1) / (2 * modeNum);
                double dec_Global = (2.0 * modeNum * modeNum - 1) / (2 * modeNum * modeNum);
                double P_Global = 1.0 / modeNum;    // initial value
                double[] P_Local = new double[modeNum];

                IterCount noImprove = maxNoImproveCount;
                Move bestMove = new Move();
                for (; (noImprove > 0) && (iterCount < solver.Config.maxIterCount); iterCount++) {
                    int modeSelect = startMode;
                    MoveMode bestMoveMode = MoveMode.Length;
                    bool isBlockSwapSelected = false;
                    bestMove.delta = DefaultPenalty.ForbiddenMove;
                    // judge every neighborhood whether to select and search when selected
                    // start from big end to make sure block swap will be tested before swap
                    for (int i = modeNum; i-- > 0; ) {
                        if (solver.rand.NextDouble() < (P_Global + P_Local[i])) { // selected
                            isBlockSwapSelected |= (findBestMove_Search[i] == findBestMove[(int)MoveMode.BlockSwap]);
                            if ((findBestMove_Search[i] != findBestMove[(int)MoveMode.Swap]) || !isBlockSwapSelected) {
                                findBestMove_Search[i](ref bestMove);
                            }
                            if (bestMoveMode != bestMove.mode) {
                                bestMoveMode = bestMove.mode;
                                modeSelect = i;
                            }
                        }
                    }

                    // no one is selected
                    while (bestMove.delta >= DefaultPenalty.MaxObjValue) {
                        findBestMove_Search[modeSelect](ref bestMove);
                        if (bestMove.delta >= DefaultPenalty.MaxObjValue) { modeSelect++; }
                        modeSelect %= modeNum;
                    }
#if INRC2_USE_TABU
                    // update tabu list first because it requires original assignment
                    updateTabu[bestMove.ModeIndex](ref bestMove);
#endif
                    applyBasicMove(ref bestMove);

                    if (updateOptima()) {   // improved
                        noImprove = maxNoImproveCount;
                        P_Global = (P_Global * dec_Global);
                        P_Local[modeSelect] += (amp_Local * (maxP_Local - P_Local[modeSelect]));
                    } else {    // not improved
                        noImprove--;
                        startMode++;
                        startMode %= modeNum;
                        P_Global += (amp_Global * (maxP_Global - P_Global));
                        P_Local[modeSelect] = (P_Local[modeSelect] * dec_Local);
                    }
                }
#if INRC2_PERFORMANCE_TEST
                IterCount iterDelta = (iterCount - startIterCount);
                Console.WriteLine("[tabuSearch_Possibility] speed: "
                    + (iterDelta * Durations.MillisecondsInSecond / clock.ElapsedMilliseconds)
                    + " iter: " + iterDelta + " time: " + clock.ElapsedMilliseconds);
#endif
            }
            /// <summary>
            /// try add shift until there is no improvement , then try change shift,
            /// then try remove shift, then try add shift again. if all of them
            /// can't improve or time is out, return.
            /// </summary>
            public void localSearch(FindBestMove[] findBestMove_Search) {
#if INRC2_PERFORMANCE_TEST
                IterCount startIterCount = iterCount;
                Stopwatch clock = new Stopwatch();
                clock.Start();
#endif
                Optima = this;

                int failCount = findBestMove_Search.Length;
                int modeSelect = 0;
                Move bestMove = new Move();
                while ((failCount > 0) && (iterCount != solver.Config.maxIterCount)) {
                    bestMove.delta = DefaultPenalty.ForbiddenMove;
                    findBestMove_Search[modeSelect](ref bestMove);
                    if (bestMove.delta < DefaultPenalty.MaxObjValue) {
                        applyBasicMove(ref bestMove);
                        updateOptima();
                        iterCount++;
                        failCount = findBestMove_Search.Length;
                    } else {
                        failCount--;
                        modeSelect++;
                        modeSelect %= findBestMove_Search.Length;
                    }
                }
#if INRC2_PERFORMANCE_TEST
                IterCount iterDelta = (iterCount - startIterCount);
                Console.WriteLine("[localSearch] speed: "
                    + (iterDelta * Durations.MillisecondsInSecond / clock.ElapsedMilliseconds)
                    + " iter: " + iterDelta + " time: " + clock.ElapsedMilliseconds);
#endif
            }
            /// <summary> randomly apply add, change or remove shift for $stepNum times. </summary>
            public void randomWalk(IterCount stepNum) {
#if INRC2_PERFORMANCE_TEST
                IterCount startIterCount = iterCount;
                Stopwatch clock = new Stopwatch();
                clock.Start();
#endif
                Optima = this;

                stepNum += iterCount;
                Move move = new Move();
                while ((iterCount < stepNum) && (iterCount < solver.Config.maxIterCount)) {
                    move.mode = (MoveMode)solver.rand.Next((int)MoveMode.BasicMovesLength);
                    move.weekday = Weekdays.Mon + solver.rand.Next(Weekdays.Num);
                    move.weekday2 = Weekdays.Mon + solver.rand.Next(Weekdays.Num);
                    if (move.weekday > move.weekday2) { Util.Swap(ref move.weekday, ref move.weekday2); }
                    move.nurse = NurseIDs.Begin + solver.rand.Next(S.NurseNum);
                    move.nurse2 = NurseIDs.Begin + solver.rand.Next(S.NurseNum);
                    move.assign.shift = ShiftIDs.Begin + solver.rand.Next(S.ShiftNum);
                    move.assign.skill = SkillIDs.Begin + solver.rand.Next(S.SkillNum);

                    move.delta = tryMove[move.ModeIndex](ref move);
                    if (move.delta < DefaultPenalty.MaxObjValue) {
                        applyBasicMove(ref move);
                        iterCount++;
                    }
                }
#if INRC2_PERFORMANCE_TEST
                IterCount iterDelta = (iterCount - startIterCount);
                Console.WriteLine("[randomWalk] speed: "
                    + (iterDelta * Durations.MillisecondsInSecond / clock.ElapsedMilliseconds)
                    + " iter: " + iterDelta + " time: " + clock.ElapsedMilliseconds);
#endif
            }

            /// <summary> change solution structure in certain complexity. </summary>
            public void perturb(double strength) {
                // TODO[3] : make this change solution structure in certain complexity
                int randomWalkStepCount = (int)(strength * Weekdays.Num
                    * S.ShiftNum * S.NurseNum);

                randomWalk(randomWalkStepCount);

                updateOptima();
            }

            /// <summary> check if the result of incremental update, evaluate and checkObjValue is the same. </summary>
            [Conditional("INRC2_DEBUG")]
            public void checkIncrementalUpdate() {
                ObjValue incrementalVal = ObjValue;
                evaluateObjValue();
                if (solver.checkFeasibility(this) != 0) {
                    solver.errorLog("infeasible solution @" + iterCount);
                }
                ObjValue checkResult = solver.checkObjValue(this);
                if (checkResult != ObjValue) {
                    solver.errorLog("check conflict with evaluate @" + iterCount);
                }
                if (ObjValue != incrementalVal) {
                    solver.errorLog("evaluate conflict with incremental update @" + iterCount);
                }
            }

            /// <summary> 
            /// set weights of nurses with less penalty to 0. 
            /// each contract will got same ratio of nurses with weight changed.
            /// </summary>
            /// <remarks> attention that rebuild() with clear the effect of this method. </remarks>
            public void adjustWeightToBiasNurseWithGreaterPenalty() {
                int biasedNurseNum = 0;
                int targetNurseNum;
                nurseWeights.fill(0);

                ObjValue[] nurseObj = new ObjValue[S.nurses.Length];
                for (NurseID nurse = 0; nurse < S.nurses.Length; nurse++) {
                    nurseObj[nurse] += evaluateConsecutiveShift(nurse);
                    nurseObj[nurse] += evaluateConsecutiveDay(nurse);
                    nurseObj[nurse] += evaluateConsecutiveDayOff(nurse);
                    nurseObj[nurse] += evaluatePreference(nurse);
                    nurseObj[nurse] += evaluateCompleteWeekend(nurse);
                    nurseObj[nurse] += evaluateTotalAssign(nurse);
                    nurseObj[nurse] += evaluateTotalWorkingWeekend(nurse);
                }

                for (ContractID c = ContractIDs.Begin; c < S.contracts.Length; c++) {
                    NurseID[] nurses = S.contracts[c].nurses;
                    Array.Sort(nurses, (lhs, rhs) => { return (nurseObj[lhs] - nurseObj[rhs]); });
                    targetNurseNum = nurses.Length / solver.Config.inversePenaltyBiasRatio;
                    biasedNurseNum += targetNurseNum;
                    for (int i = 0; i < targetNurseNum; i++) {
                        nurseWeights[nurses[i]] = 1;
                    }
                    if (solver.rand.Next(solver.Config.inversePenaltyBiasRatio)
                        < (nurses.Length % solver.Config.inversePenaltyBiasRatio)) {
                        nurseWeights[nurses[targetNurseNum]] = 1;
                        biasedNurseNum++;
                    }
                }

                // pick nurse randomly to meet the TotalBiasRatio
                targetNurseNum = S.nurses.Length / solver.Config.inverseTotalBiasRatio;
                while (biasedNurseNum < targetNurseNum) {
                    NurseID nurse = solver.rand.Next(S.nurses.Length);
                    if (nurseWeights[nurse] == 0) {
                        nurseWeights[nurse] = 1;
                        biasedNurseNum++;
                    }
                }
            }

            /// <summary>
            /// set assignments to output.AssignTable with about $differentSlotNum
            /// of assignment is different and rebuild assist data.
            /// (output must be build from same problem)
            /// </summary>
            /// <remarks> objValue will be recalculated. </remarks>
            public void rebuild(Output output, int differentSlotNum) {
                int totalSlotNum = S.NurseNum * Weekdays.Num;

                if (differentSlotNum < totalSlotNum) {
                    Output assignments = ((this == output) ? new Output(output) : output);

                    resetAssign();
                    resetAssistData();

                    for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                        for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                            if (assignments[nurse, weekday].IsWorking) {
                                if (solver.rand.Next(totalSlotNum) > differentSlotNum) {
                                    addAssign(weekday, nurse, assignments[nurse, weekday]);
                                }
                            }
                        }
                    }
                }

                repair_AlwaysEvaluateObjValue();
            }
            /// <summary> 
            /// set assignments to output.AssignTable and rebuild assist data.
            /// (output must be build from same problem)
            /// </summary>
            /// <remarks> objValue is copied from output. </remarks>
            public void rebuild(Output output) {
                Output assignments = ((this == output) ? new Output(output) : output);

                resetAssign();
                resetAssistData();

                for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                    for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                        if (assignments[nurse, weekday].IsWorking) {
                            addAssign(weekday, nurse, assignments[nurse, weekday]);
                        }
                    }
                }

                ObjValue = output.ObjValue;
            }
            /// <remarks> 
            /// must be called after direct access to AssignTable. 
            /// objValue will be recalculated.
            /// </remarks>
            public void rebuild() {
                rebuild(this);
                evaluateObjValue();
            }

            /// <summary> make infeasible solution feasible. </summary>
            public void repair(bool isAlwaysEvaluateObjValue = false) {
#if INRC2_PERFORMANCE_TEST
                IterCount startIterCount = iterCount;
                Stopwatch clock = new Stopwatch();
                clock.Start();
#endif
                // must not use swap for swap mode is not compatible with repair mode
                // also, the repair procedure doesn't need the technique to jump through infeasible solutions
                FindBestMove[] findBestMove_Repair = { this.findBestARBoth, this.findBestChange };

                ObjValue violation = solver.checkFeasibility(this);

                if (violation == 0) {
                    if (isAlwaysEvaluateObjValue) { evaluateObjValue(); }
                    return;
                }

                #region reduced tabuSearch_Rand()
                penalty.setRepairMode();

                ObjValue = violation;

                const int minWeight = 256;  // min weight
                const int maxWeight = 1024; // max weight (less than (RAND_MAX / findBestMove_Repair.Length))
                const int initWeight = (maxWeight + minWeight) / 2;
                const int deltaIncRatio = 8;    // = weights[mode] / weightDelta
                const int incError = deltaIncRatio - 1;
                const int deltaDecRatio = 8;    // = weights[mode] / weightDelta
                const int decError = -(deltaDecRatio - 1);
                int[] weights = new int[findBestMove_Repair.Length];
                int totalWeight = initWeight * findBestMove_Repair.Length;

                for (; (ObjValue > 0) && (iterCount < solver.config.maxIterCount); iterCount++) {
                    int modeSelect = 0;
                    for (int w = solver.rand.Next(totalWeight); (w -= weights[modeSelect]) >= 0; modeSelect++) { }

                    Move bestMove = new Move();
                    findBestMove_Repair[modeSelect](ref bestMove);

#if INRC2_USE_TABU
                    // update tabu list first because it requires original assignment
                    updateTabu[bestMove.ModeIndex](ref bestMove);
#endif
                    int weightDelta;
                    if (bestMove.delta < DefaultPenalty.MaxObjValue) {
                        applyBasicMove(ref bestMove);

                        if (bestMove.delta < 0) {    // improve current solution
                            weightDelta = (incError + maxWeight - weights[modeSelect]) / deltaIncRatio;
                        } else {    // no improve
                            weightDelta = (decError + minWeight - weights[modeSelect]) / deltaDecRatio;
                        }
                    } else {    // invalid
                        weightDelta = (decError + minWeight - weights[modeSelect]) / deltaDecRatio;
                    }

                    weights[modeSelect] += weightDelta;
                    totalWeight += weightDelta;
                }

                penalty.recoverLastMode();
                #endregion reduced tabuSearch_Rand()

                violation = ObjValue;
                evaluateObjValue();

#if INRC2_PERFORMANCE_TEST
                Console.WriteLine("[repair] " + ((violation == 0) ? "(success)" : "(fail)")
                    + " iter: " + (iterCount - startIterCount) + " time: " + clock.ElapsedMilliseconds);
#endif
            }
            public void repair_AlwaysEvaluateObjValue() {
                repair(true);
            }

            /// <summary> set data structure to default value. </summary>
            /// <remarks> must be called before building up a solution. </remarks> 
            private void resetAssign() {
                for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                    for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                        AssignTable[nurse, weekday].shift = ShiftIDs.None;
                    }
                }
            }
            /// <summary> set data structure to default value. </summary>
            /// <remarks> must be called before building up a solution. </remarks> 
            private void resetAssistData() {
                totalAssignNums.fill(0);
                Array.Copy(W.optNurseNums, missingNurseNums, W.optNurseNums.Length);
                Array.Clear(assignedNurseNums, 0, assignedNurseNums.Length);
                for (NurseID nurse = 0; nurse < S.NurseNum; nurse++) {
                    consecutives_Backup[nurse].copyTo(consecutives[nurse]);
                }
                nurseWeights.fill(1);   // TUNEUP[6]: leave it out if not using it.
                isBlockSwapCacheValid.fill(false);

#if INRC2_USE_TABU
                iterCount += (solver.ShiftTabuTenureBase + solver.ShiftTabuTenureAmp
                    + solver.DayTabuTenureBase + solver.DayTabuTenureAmp);
#endif // INRC2_USE_TABU
            }

            /// <summary> for each skill on each shift on each day, try assign each nurse to it or not. </summary>
            /// <param name="nurseNum"> number of nurses who were assigned to current slot. </param>
            /// <returns> true if all slots have been filled. </returns>
            private bool fillAssign(Weekday weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum) {
                if ((S.nurses.Length - nurse + nurseNum)
                    < W.minNurseNums[weekday, shift, skill]) {
                    return false;
                } else if (nurse >= S.nurses.Length) {
                    return fillAssign(weekday, shift, skill + 1, NurseIDs.Begin, 0);
                } else if (skill >= S.SkillsLength) {
                    return fillAssign(weekday, shift + 1, SkillIDs.Begin, NurseIDs.Begin, 0);
                } else if (shift >= S.shifts.Length) {
                    return fillAssign(weekday + 1, ShiftIDs.Begin, SkillIDs.Begin, NurseIDs.Begin, 0);
                } else if (weekday > Weekdays.Sun) {
                    return true;
                }

                Assign firstAssign = new Assign(shift, skill);
                Assign secondAssign = new Assign();
                NurseID firstNurseNum = nurseNum + 1;
                NurseID secondNurseNum = nurseNum;
                bool isNotAssignedBefore = !AssignTable[nurse, weekday].IsWorking;

                if (isNotAssignedBefore) {
                    if (S.nurses[nurse].skills[skill]
                        && isValidSuccession(nurse, weekday, shift)) {
                        if (solver.rand.Next(2) == 0) {
                            Util.Swap(ref firstAssign, ref secondAssign);
                            Util.Swap(ref firstNurseNum, ref secondNurseNum);
                        }

                        AssignTable[nurse, weekday] = firstAssign;
                        if (fillAssign(weekday, shift, skill, nurse + 1, firstNurseNum)) {
                            return true;
                        }
                    }

                    AssignTable[nurse, weekday] = secondAssign;
                }

                if (fillAssign(weekday, shift, skill, nurse + 1, secondNurseNum)) {
                    return true;
                } else if (isNotAssignedBefore) {
                    AssignTable[nurse, weekday].shift = ShiftIDs.None;
                }

                return false;
            }

            /// <summary> evaluate objective by assist data structure. </summary>
            /// <remarks> must be called after Penalty change or direct access to AssignTable. </remarks>
            private void evaluateObjValue() {
                evaluateObjValue_IgnoreSpanningConstraint();
                for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                    ObjValue += evaluateTotalAssign(nurse);
                    ObjValue += evaluateTotalWorkingWeekend(nurse);
                }
            }
            /// <summary> 
            /// evaluate objective by assist data structure. 
            /// (TotalAssign and TotalWorkingWeekend is ignored)
            /// </summary>
            private void evaluateObjValue_IgnoreSpanningConstraint() {
                ObjValue += evaluateInsufficientStaff();
                for (NurseID nurse = NurseIDs.Begin; nurse < S.nurses.Length; nurse++) {
                    ObjValue += evaluateConsecutiveShift(nurse);
                    ObjValue += evaluateConsecutiveDay(nurse);
                    ObjValue += evaluateConsecutiveDayOff(nurse);
                    ObjValue += evaluatePreference(nurse);
                    ObjValue += evaluateCompleteWeekend(nurse);
                }
            }
            private ObjValue evaluateInsufficientStaff() {
                ObjValue obj = 0;

                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    for (ShiftID shift = ShiftIDs.Begin; shift < S.shifts.Length; shift++) {
                        for (SkillID skill = SkillIDs.Begin; skill < S.SkillsLength; skill++) {
                            if (missingNurseNums[weekday, shift, skill] > 0) {
                                obj += penalty.InsufficientStaff * missingNurseNums[weekday, shift, skill];
                            }
                        }
                    }
                }

                return obj;
            }
            private ObjValue evaluateConsecutiveShift(NurseID nurse) {
                ObjValue obj = 0;

                Consecutive c = consecutives[nurse];
                Problem.ScenarioInfo.Shift[] sh = S.shifts;

                int nextday = c.shiftHigh[Weekdays.Mon] + 1;
                if (nextday <= Weekdays.Sun) {   // the entire week is not one block
                    // handle first block with history
                    if (AssignTable[nurse, Weekdays.Mon].IsWorking) {
                        ShiftID shid = AssignTable[nurse, Weekdays.Mon].shift;
                        if (H.lastShifts[nurse] == AssignTable[nurse, Weekdays.Mon].shift) {
                            if (H.consecutiveShiftNums[nurse] > sh[shid].maxConsecutiveShiftNum) {
                                // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                                obj += penalty.ConsecutiveShift * (c.shiftHigh[Weekdays.Mon] - Weekdays.Mon + 1);
                            } else {
                                obj += penalty.ConsecutiveShift * Util.distanceToRange(
                                    c.shiftHigh[Weekdays.Mon] - c.shiftLow[Weekdays.Mon] + 1,
                                    sh[shid].minConsecutiveShiftNum, sh[shid].maxConsecutiveShiftNum);
                            }
                        } else {
                            obj += penalty.ConsecutiveShift * Util.distanceToRange(c.shiftHigh[Weekdays.Mon] - Weekdays.Mon + 1,
                                sh[shid].minConsecutiveShiftNum, sh[shid].maxConsecutiveShiftNum);
                            if (ShiftIDs.isWorking(H.lastShifts[nurse])) {
                                obj += penalty.ConsecutiveShift * Util.absentCount(H.consecutiveShiftNums[nurse],
                                    sh[H.lastShifts[nurse]].minConsecutiveShiftNum);
                            }
                        }
                    } else if (ShiftIDs.isWorking(H.lastShifts[nurse])) {
                        obj += penalty.ConsecutiveShift * Util.absentCount(
                            H.consecutiveShiftNums[nurse], sh[H.lastShifts[nurse]].minConsecutiveShiftNum);
                    }
                    // handle blocks in the middle of the week
                    for (; c.shiftHigh[nextday] < Weekdays.Sun; nextday = c.shiftHigh[nextday] + 1) {
                        if (AssignTable[nurse, nextday].IsWorking) {
                            ShiftID shid = AssignTable[nurse, nextday].shift;
                            obj += penalty.ConsecutiveShift *
                                Util.distanceToRange(c.shiftHigh[nextday] - c.shiftLow[nextday] + 1,
                                sh[shid].minConsecutiveShiftNum, sh[shid].maxConsecutiveShiftNum);
                        }
                    }
                }
                // handle last consecutive block
                int consecutiveShift_EntireWeek = H.consecutiveShiftNums[nurse] + Weekdays.Num;
                int consecutiveShift = c.shiftHigh[Weekdays.Sun] - c.shiftLow[Weekdays.Sun] + 1;
                if (AssignTable[nurse, Weekdays.Sun].IsWorking) {
                    ShiftID shid = AssignTable[nurse, Weekdays.Sun].shift;
                    if (c.IsSingleConsecutiveShift) { // the entire week is one block
                        if (H.lastShifts[nurse] == AssignTable[nurse, Weekdays.Sun].shift) {
                            if (H.consecutiveShiftNums[nurse] > sh[shid].maxConsecutiveShiftNum) {
                                obj += penalty.ConsecutiveShift * Weekdays.Num;
                            } else {
                                obj += penalty.ConsecutiveShift * Util.exceedCount(
                                    consecutiveShift_EntireWeek, sh[shid].maxConsecutiveShiftNum);
                            }
                        } else {    // different shifts
                            if (Weekdays.Num > sh[shid].maxConsecutiveShiftNum) {
                                obj += penalty.ConsecutiveShift *
                                    (Weekdays.Num - sh[shid].maxConsecutiveShiftNum);
                            }
                            if (ShiftIDs.isWorking(H.lastShifts[nurse])) {
                                obj += penalty.ConsecutiveShift * Util.absentCount(H.consecutiveShiftNums[nurse],
                                    sh[H.lastShifts[nurse]].minConsecutiveShiftNum);
                            }
                        }
                    } else {
                        obj += penalty.ConsecutiveShift * Util.exceedCount(
                            consecutiveShift, sh[shid].maxConsecutiveShiftNum);
                    }
                } else if (c.IsSingleConsecutiveShift // the entire week is one block
                    && ShiftIDs.isWorking(H.lastShifts[nurse])) {
                    obj += penalty.ConsecutiveShift * Util.absentCount(
                        H.consecutiveShiftNums[nurse], sh[H.lastShifts[nurse]].minConsecutiveShiftNum);
                }

                return obj;
            }
            private ObjValue evaluateConsecutiveDay(NurseID nurse) {
                ObjValue obj = 0;

                Consecutive c = consecutives[nurse];
                ContractID cid = S.nurses[nurse].contract;
                Problem.ScenarioInfo.Contract[] cs = S.contracts;

                int nextday = c.dayHigh[Weekdays.Mon] + 1;
                if (nextday <= Weekdays.Sun) {   // the entire week is not one block
                    // handle first block with history
                    if (AssignTable[nurse, Weekdays.Mon].IsWorking) {
                        if (H.consecutiveDayNums[nurse] > cs[cid].maxConsecutiveDayNum) {
                            // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                            obj += penalty.ConsecutiveDay * (c.dayHigh[Weekdays.Mon] - Weekdays.Mon + 1);
                        } else {
                            obj += penalty.ConsecutiveDay * Util.distanceToRange(
                                c.dayHigh[Weekdays.Mon] - c.dayLow[Weekdays.Mon] + 1,
                                cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        }
                    } else if (ShiftIDs.isWorking(H.lastShifts[nurse])) {
                        obj += penalty.ConsecutiveDay * Util.absentCount
                            (H.consecutiveDayNums[nurse], cs[cid].minConsecutiveDayNum);
                    }
                    // handle blocks in the middle of the week
                    for (; c.dayHigh[nextday] < Weekdays.Sun; nextday = c.dayHigh[nextday] + 1) {
                        if (AssignTable[nurse, nextday].IsWorking) {
                            obj += penalty.ConsecutiveDay *
                                Util.distanceToRange(c.dayHigh[nextday] - c.dayLow[nextday] + 1,
                                cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        }
                    }
                }
                // handle last consecutive block
                int consecutiveDay = c.dayHigh[Weekdays.Sun] - c.dayLow[Weekdays.Sun] + 1;
                if (AssignTable[nurse, Weekdays.Sun].IsWorking) {
                    if (c.IsSingleConsecutiveDay) {   // the entire week is one block
                        if (H.consecutiveDayNums[nurse] > cs[cid].maxConsecutiveDayNum) {
                            obj += penalty.ConsecutiveDay * Weekdays.Num;
                        } else {
                            obj += penalty.ConsecutiveDay * Util.exceedCount(
                                consecutiveDay, cs[cid].maxConsecutiveDayNum);
                        }
                    } else {
                        obj += penalty.ConsecutiveDay * Util.exceedCount(
                            consecutiveDay, cs[cid].maxConsecutiveDayNum);
                    }
                } else if (c.IsSingleConsecutiveDay // the entire week is one block
                    && ShiftIDs.isWorking(H.lastShifts[nurse])) {
                    obj += penalty.ConsecutiveDay * Util.absentCount(
                        H.consecutiveDayNums[nurse], cs[cid].minConsecutiveDayNum);
                }

                return obj;
            }
            private ObjValue evaluateConsecutiveDayOff(NurseID nurse) {
                ObjValue obj = 0;

                Consecutive c = consecutives[nurse];
                ContractID cid = S.nurses[nurse].contract;
                Problem.ScenarioInfo.Contract[] cs = S.contracts;

                int nextday = c.dayHigh[Weekdays.Mon] + 1;
                if (nextday <= Weekdays.Sun) {   // the entire week is not one block
                    // handle first block with history
                    if (!AssignTable[nurse, Weekdays.Mon].IsWorking) {
                        if (H.consecutiveDayoffNums[nurse] > cs[cid].maxConsecutiveDayoffNum) {
                            obj += penalty.ConsecutiveDayOff * (c.dayHigh[Weekdays.Mon] - Weekdays.Mon + 1);
                        } else {
                            obj += penalty.ConsecutiveDayOff * Util.distanceToRange(
                                c.dayHigh[Weekdays.Mon] - c.dayLow[Weekdays.Mon] + 1,
                                cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        }
                    } else if (!ShiftIDs.isWorking(H.lastShifts[nurse])) {
                        obj += penalty.ConsecutiveDayOff * Util.absentCount(
                            H.consecutiveDayoffNums[nurse], cs[cid].minConsecutiveDayoffNum);
                    }
                    // handle blocks in the middle of the week
                    for (; c.dayHigh[nextday] < Weekdays.Sun; nextday = c.dayHigh[nextday] + 1) {
                        if (!AssignTable[nurse, nextday].IsWorking) {
                            obj += penalty.ConsecutiveDayOff *
                                Util.distanceToRange(c.dayHigh[nextday] - c.dayLow[nextday] + 1,
                                cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        }
                    }
                }
                // handle last consecutive block
                int consecutiveDay = c.dayHigh[Weekdays.Sun] - c.dayLow[Weekdays.Sun] + 1;
                if (!AssignTable[nurse, Weekdays.Sun].IsWorking) {
                    if (c.IsSingleConsecutiveDay) {   // the entire week is one block
                        if (H.consecutiveDayoffNums[nurse] > cs[cid].maxConsecutiveDayoffNum) {
                            obj += penalty.ConsecutiveDayOff * Weekdays.Num;
                        } else {
                            obj += penalty.ConsecutiveDayOff * Util.exceedCount(
                                consecutiveDay, cs[cid].maxConsecutiveDayoffNum);
                        }
                    } else {
                        obj += penalty.ConsecutiveDayOff * Util.exceedCount(
                            consecutiveDay, cs[cid].maxConsecutiveDayoffNum);
                    }
                } else if (c.IsSingleConsecutiveDay // the entire week is one block
                    && (!ShiftIDs.isWorking(H.lastShifts[nurse]))) {
                    obj += penalty.ConsecutiveDayOff * Util.absentCount(
                    H.consecutiveDayoffNums[nurse], cs[cid].minConsecutiveDayoffNum);
                }

                return obj;
            }
            private ObjValue evaluatePreference(NurseID nurse) {
                ObjValue obj = 0;

                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    ShiftID shift = AssignTable[nurse, weekday].shift;
                    if (W.shiftOffs[weekday, shift, nurse]) {
                        obj += penalty.Preference;
                    }
                }

                return obj;
            }
            private ObjValue evaluateCompleteWeekend(NurseID nurse) {
                ObjValue obj = 0;

                if (S.contracts[S.nurses[nurse].contract].completeWeekend
                    && (AssignTable[nurse, Weekdays.Sat].IsWorking != AssignTable[nurse, Weekdays.Sun].IsWorking)) {
                    obj += penalty.CompleteWeekend;
                }

                return obj;
            }
            private ObjValue evaluateTotalAssign(NurseID nurse) {
                ObjValue obj = 0;

                obj += penalty.TotalAssign * Util.distanceToRange(
                    totalAssignNums[nurse] * H.restWeekCount,
                    solver.restMinShiftNum[nurse], solver.restMaxShiftNum[nurse])
                    / H.restWeekCount;

                return obj;
            }
            private ObjValue evaluateTotalWorkingWeekend(NurseID nurse) {
                ObjValue obj = 0;

                int workingWeekendNum = (AssignTable[nurse, Weekdays.Sat].IsWorking
                    || AssignTable[nurse, Weekdays.Sun].IsWorking) ? 1 : 0;
#if INRC2_AVERAGE_MAX_WORKING_WEEKEND
                int maxWeekend = S.contracts[S.nurses[nurse].contract].maxWorkingWeekendNum;
                int historyWeekend = H.totalWorkingWeekendNums[nurse] * S.totalWeekNum;
                int exceedingWeekend = historyWeekend - (maxWeekend * H.currentWeek)
                    + (workingWeekendNum * S.totalWeekNum);
                if (exceedingWeekend > 0) {
                    obj += penalty.TotalWorkingWeekend * exceedingWeekend / S.totalWeekNum;
                }
#else
                obj += penalty.TotalWorkingWeekend * Util.exceedCount(
                    workingWeekendNum * H.restWeekCount,
                    solver.restMaxWorkingWeekendNum[nurse]) / H.restWeekCount;
#endif

                return obj;
            }

            // UNDONE[0]: parameter sequence has been changed!
            private bool isValidSuccession(NurseID nurse, Weekday weekday, ShiftID shift) {
                return S.shifts[AssignTable[nurse, weekday - 1].shift].legalNextShifts[shift];
            }
            // UNDONE[0]: parameter sequence has been changed!
            private bool isValidPrior(NurseID nurse, Weekday weekday, ShiftID shift) {
                return S.shifts[shift].legalNextShifts[AssignTable[nurse, weekday + 1].shift];
            }

            /// <summary> reset all cache valid flag of corresponding nurses to false. </summary>
            private void invalidateCacheFlag(ref Move move) {
                isBlockSwapCacheValid[move.nurse] = false;
                if ((move.mode == MoveMode.BlockSwap)
                    || (move.mode == MoveMode.Swap)) {
                    isBlockSwapCacheValid[move.nurse2] = false;
                }
            }

            /// <summary> return true if update succeed. </summary>
            private bool updateOptima() {
#if INRC2_SECONDARY_OBJ_VALUE
                if (ObjValue <= Optima.ObjValue) {
                    SecondaryObjValue = 0;
                    for (NurseID n = 0; n < S.nurses.Length; n++) {
                        SecondaryObjValue += ((SecondaryObjValue)(totalAssignNums[n])
                            / (1 + Math.Abs(solver.restMaxShiftNum[n]
                            + S.contracts[S.nurses[n].contract].maxShiftNum)));
                    }
                }
#endif
                if (ObjValue < Optima.ObjValue) {
                    FindTime = solver.clock.ElapsedTicks;
                    Optima = this;
                    return true;
                } else if (ObjValue == Optima.ObjValue) {
#if INRC2_SECONDARY_OBJ_VALUE
                    bool isSelected = (SecondaryObjValue < Optima.SecondaryObjValue);
#else
                    bool isSelected = (solver.rand.Next(2) == 0);
#endif
                    if (isSelected) {
                        FindTime = solver.clock.ElapsedTicks;
                        Optima = this;
                        return true;
                    }
                }

                return false;
            }

            /// <summary>
            // apply the moves defined in MoveMode as BasicMove,
            // update objective value and update cache valid flags.
            /// </summary>
            private void applyBasicMove(ref Move move) {
                applyMove[move.ModeIndex](ref move);
                ObjValue += move.delta;
                invalidateCacheFlag(ref move);
            }

            // BlockBorder means the start or end day of a consecutive block
            private void findBestAdd(ref Move bestMove) {
                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.Add);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.Add);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                        if (!AssignTable[move.nurse, move.weekday].IsWorking) {
                            for (move.assign.shift = ShiftIDs.Begin;
                                move.assign.shift < S.shifts.Length; move.assign.shift++) {
                                for (move.assign.skill = SkillIDs.Begin;
                                    move.assign.skill < S.SkillsLength; move.assign.skill++) {
                                    move.delta = tryAddAssign(ref move);
#if INRC2_USE_TABU
                                    if (noAddTabu(ref move)) {
#endif
                                        if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                            bestMove = move;
                                        }
#if INRC2_USE_TABU
                                    } else {    // tabu
                                        if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                            bestMove_tabu = move;
                                        }
                                    }
#endif
                                }
                            }
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif
            }
            private void findBestRemove(ref Move bestMove) {
                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.Remove);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.Remove);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                        if (AssignTable[move.nurse, move.weekday].IsWorking) {
                            move.delta = tryRemoveAssign(ref move);
#if INRC2_USE_TABU
                            if (noRemoveTabu(ref move)) {
#endif
                                if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                    bestMove = move;
                                }
#if INRC2_USE_TABU
                            } else {    // tabu
                                if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                    bestMove_tabu = move;
                                }
                            }
#endif
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif
            }
            private void findBestChange(ref Move bestMove) {
                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.Change);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.Change);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                        if (AssignTable[move.nurse, move.weekday].IsWorking) {
                            for (move.assign.shift = ShiftIDs.Begin;
                                move.assign.shift < S.shifts.Length; move.assign.shift++) {
                                for (move.assign.skill = SkillIDs.Begin;
                                    move.assign.skill < S.SkillsLength; move.assign.skill++) {
                                    move.delta = tryChangeAssign(ref move);
#if INRC2_USE_TABU
                                    if (noChangeTabu(ref move)) {
#endif
                                        if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                            bestMove = move;
                                        }
#if INRC2_USE_TABU
                                    } else {    // tabu
                                        if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                            bestMove_tabu = move;
                                        }
                                    }
#endif
                                }
                            }
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif
            }
            private void findBestSwap(ref Move bestMove) {
                penalty.setSwapMode();

                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.Swap);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.Swap);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.nurse2 = move.nurse + 1; move.nurse2 < S.nurses.Length; move.nurse2++) {
                        if ((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0)) {
                            continue;
                        }
                        for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                            move.delta = trySwapNurse(move.weekday, move.nurse, move.nurse2);
#if INRC2_USE_TABU
                            if (noSwapTabu(ref move)) {
#endif
                                if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                    bestMove = move;
                                }
#if INRC2_USE_TABU
                            } else {    // tabu
                                if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                    bestMove_tabu = move;
                                }
                            }
#endif
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif

                penalty.recoverLastMode();
            }
            private void findBestBlockSwap_Cached(ref Move bestMove) {  // try all nurses
                penalty.setBlockSwapMode();

                Util.RandSelect rs = new Util.RandSelect();

                Move move = new Move(MoveMode.BlockSwap);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.nurse2 = move.nurse + 1; move.nurse2 < S.nurses.Length; move.nurse2++) {
                        if (((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0))
                            || !solver.haveSameSkill(move.nurse, move.nurse2)) {
                            continue;
                        }
                        BlockSwapCacheItem cache = blockSwapCache[move.nurse, move.nurse2];
                        if (!(isBlockSwapCacheValid[move.nurse] && isBlockSwapCacheValid[move.nurse2])) {
                            Util.RandSelect rs_currentPair = new Util.RandSelect();
                            cache.delta = DefaultPenalty.ForbiddenMove;
                            for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                                move.delta = trySwapBlock(move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                                if (rs_currentPair.isMinimal(move.delta, cache.delta, solver.rand.Next())) {
                                    cache.delta = move.delta;
                                    cache.weekday = move.weekday;
                                    cache.weekday2 = move.weekday2;
                                }
                            }
                        }
                        if (rs.isMinimal(cache.delta, bestMove.delta, solver.rand.Next())) {
                            bestMove.mode = MoveMode.BlockSwap;
                            bestMove.delta = cache.delta;
                            bestMove.nurse = move.nurse;
                            bestMove.nurse2 = move.nurse2;
                            bestMove.weekday = cache.weekday;
                            bestMove.weekday2 = cache.weekday2;
                        }
                    }
                    isBlockSwapCacheValid[move.nurse] = true;
                }

                penalty.recoverLastMode();
            }
#if !INRC2_BLOCK_SWAP_CACHED
            private void findBestBlockSwap_Base(ref Move bestMove) {         // try all nurses
                penalty.setBlockSwapMode();

                NurseID maxNurseID = S.nurses.Length - 1;
                Util.RandSelect rs = new Util.RandSelect();

                Move move = new Move(MoveMode.BlockSwap);
                move.nurse = findBestBlockSwap_StartNurse;
                for (NurseID count = S.nurses.Length; count > 0; count--) {
                    move.nurse = ((move.nurse < maxNurseID) ? (move.nurse + 1) : 0);
                    move.nurse2 = move.nurse;
                    for (NurseID count2 = count - 1; count2 > 0; count2--) {
                        move.nurse2 = ((move.nurse2 < maxNurseID) ? (move.nurse2 + 1) : 0);
                        if ((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0)) {
                            continue;
                        }
                        if (solver.haveSameSkill(move.nurse, move.nurse2)) {
                            for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                                move.delta = trySwapBlock(move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                                if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                    bestMove = move;
#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
                                    if (bestMove.delta < 0) { goto RecordAndRecover; }
#endif
                                }
                            }
                        }
                    }
                }

#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
RecordAndRecover:
#endif
                findBestBlockSwap_StartNurse = move.nurse;
                penalty.recoverLastMode();
            }
            private void findBestBlockSwap_Fast(ref Move bestMove) {    // try all nurses
                penalty.setBlockSwapMode();

                NurseID maxNurseID = S.nurses.Length - 1;
                Util.RandSelect rs = new Util.RandSelect();

                Move move = new Move(MoveMode.BlockSwap);
                move.nurse = findBestBlockSwap_StartNurse;
                for (NurseID count = S.nurses.Length; count > 0; count--) {
                    move.nurse = ((move.nurse < maxNurseID) ? (move.nurse + 1) : 0);
                    move.nurse2 = move.nurse;
                    for (NurseID count2 = count - 1; count2 > 0; count2--) {
                        move.nurse2 = ((move.nurse2 < maxNurseID) ? (move.nurse2 + 1) : 0);
                        if ((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0)) {
                            continue;
                        }
                        if (solver.haveSameSkill(move.nurse, move.nurse2)) {
                            move.delta = trySwapBlock_Fast(ref move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                            if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                bestMove = move;
#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
                                if (bestMove.delta < 0) { goto RecordAndRecover; }
#endif
                            }
                        }
                    }
                }

#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
RecordAndRecover:
#endif
                findBestBlockSwap_StartNurse = move.nurse;
                penalty.recoverLastMode();
            }
            private void findBestBlockSwap_Part(ref Move bestMove) {    // try some nurses following index
                penalty.setBlockSwapMode();

                int nurseNum_noTry = S.nurses.Length - S.nurses.Length / 4;
                NurseID maxNurseID = S.nurses.Length - 1;

                Util.RandSelect rs = new Util.RandSelect();

                Move move = new Move(MoveMode.BlockSwap);
                move.nurse = findBestBlockSwap_StartNurse;
                for (NurseID count = S.nurses.Length; count > nurseNum_noTry; count--) {
                    move.nurse = ((move.nurse < maxNurseID) ? (move.nurse + 1) : 0);
                    move.nurse2 = move.nurse;
                    for (NurseID count2 = count - 1; count2 > 0; count2--) {
                        move.nurse2 = ((move.nurse2 < maxNurseID) ? (move.nurse2 + 1) : 0);
                        if ((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0)) {
                            continue;
                        }
                        if (solver.haveSameSkill(move.nurse, move.nurse2)) {
                            move.delta = trySwapBlock_Fast(ref move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                            if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                bestMove = move;
#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
                                if (bestMove.delta < 0) { goto RecordAndRecover; }
#endif
                            }
                        }
                    }
                }

#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
RecordAndRecover:
#endif
                findBestBlockSwap_StartNurse = move.nurse;
                penalty.recoverLastMode();
            }
            private void findBestBlockSwap_Rand(ref Move bestMove) {    // try randomly picked nurses 
                penalty.setBlockSwapMode();

                int nurseNum_noTry = S.nurses.Length - S.nurses.Length / 4;
                NurseID maxNurseID = S.nurses.Length - 1;

                Util.RandSelect rs = new Util.RandSelect();

                Move move = new Move(MoveMode.BlockSwap);
                for (NurseID count = S.nurses.Length; count > nurseNum_noTry; count--) {
                    move.nurse = NurseIDs.Begin + solver.rand.Next(S.NurseNum);
                    move.nurse2 = move.nurse;
                    for (NurseID count2 = count - 1; count2 > 0; count2--) {
                        move.nurse2 = ((move.nurse2 < maxNurseID) ? (move.nurse2 + 1) : 0);
                        if ((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0)) {
                            continue;
                        }
                        if (solver.haveSameSkill(move.nurse, move.nurse2)) {
                            move.delta = trySwapBlock_Fast(ref move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                            if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                bestMove = move;
#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
                                if (bestMove.delta < 0) { goto RecordAndRecover; }
#endif
                            }
                        }
                    }
                }

#if INRC2_BLOCK_SWAP_FIRST_IMPROVE
RecordAndRecover:
#endif
                findBestBlockSwap_StartNurse = move.nurse;
                penalty.recoverLastMode();
            }
#endif // !INRC2_BLOCK_SWAP_CACHED
            private void findBestExchange(ref Move bestMove) {
                penalty.setExchangeMode();

                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.Exchange);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.Exchange);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                        for (move.weekday2 = move.weekday + 1; move.weekday2 <= Weekdays.Sun; move.weekday2++) {
                            move.delta = tryExchangeDay(move.weekday, move.nurse, move.weekday2);
#if INRC2_USE_TABU
                            if (noExchangeTabu(ref move)) {
#endif
                                if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                    bestMove = move;
                                }
#if INRC2_USE_TABU
                            } else {    // tabu
                                if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                    bestMove_tabu = move;
                                }
                            }
#endif
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif

                penalty.recoverLastMode();
            }
            private void findBestBlockShift(ref Move bestMove) {
                penalty.setExchangeMode();

                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.Exchange);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.Exchange);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    Consecutive c = consecutives[move.nurse];
                    move.weekday = Weekdays.Mon;
                    move.weekday2 = c.dayHigh[move.weekday] + 1;
                    while (move.weekday2 <= Weekdays.Sun) {
                        move.delta = tryExchangeDay(move.weekday, move.nurse, move.weekday2);
#if INRC2_USE_TABU
                        if (noExchangeTabu(ref move)) {
#endif
                            if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                bestMove = move;
                            }
#if INRC2_USE_TABU
                        } else {    // tabu
                            if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                bestMove_tabu = move;
                            }
                        }
#endif
                        if (move.weekday != c.dayHigh[move.weekday]) {  // start of a block
                            move.weekday = c.dayHigh[move.weekday];
                            move.weekday2 = c.dayHigh[move.weekday + 1];
                        } else {    // end of a block
                            ++move.weekday;
                            move.weekday2 = c.dayHigh[move.weekday] + 1;
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif

                penalty.recoverLastMode();
            }
            private void findBestARRand(ref Move bestMove) {
                if (solver.rand.Next(2) == 0) {
                    findBestAdd(ref bestMove);
                } else {
                    findBestRemove(ref bestMove);
                }
            }
            private void findBestARBoth(ref Move bestMove) {
                Util.RandSelect rs = new Util.RandSelect();
#if INRC2_USE_TABU
                Move bestMove_tabu = new Move(MoveMode.ARBoth);
                Util.RandSelect rs_tabu = new Util.RandSelect();
#endif

                Move move = new Move(MoveMode.ARBoth);
                for (move.nurse = 0; move.nurse < S.nurses.Length; move.nurse++) {
                    for (move.weekday = Weekdays.Mon; move.weekday <= Weekdays.Sun; move.weekday++) {
                        if (AssignTable[move.nurse, move.weekday].IsWorking) {
                            move.delta = tryRemoveAssign(ref move);
#if INRC2_USE_TABU
                            if (noRemoveTabu(ref move)) {
#endif
                                if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                    bestMove = move;
                                    bestMove.mode = MoveMode.Remove;
                                }
#if INRC2_USE_TABU
                            } else {    // tabu
                                if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                    bestMove_tabu = move;
                                    bestMove_tabu.mode = MoveMode.Remove;
                                }
                            }
#endif
                        } else {
                            for (move.assign.shift = ShiftIDs.Begin;
                                move.assign.shift < S.shifts.Length; move.assign.shift++) {
                                for (move.assign.skill = SkillIDs.Begin;
                                    move.assign.skill < S.SkillsLength; move.assign.skill++) {
                                    move.delta = tryAddAssign(ref move);
#if INRC2_USE_TABU
                                    if (noAddTabu(ref move)) {
#endif
                                        if (rs.isMinimal(move.delta, bestMove.delta, solver.rand.Next())) {
                                            bestMove = move;
                                            bestMove.mode = MoveMode.Add;
                                        }
#if INRC2_USE_TABU
                                    } else {    // tabu
                                        if (rs_tabu.isMinimal(move.delta, bestMove_tabu.delta, solver.rand.Next())) {
                                            bestMove_tabu = move;
                                            bestMove_tabu.mode = MoveMode.Add;
                                        }
                                    }
#endif
                                }
                            }
                        }
                    }
                }

#if INRC2_USE_TABU
                if (aspirationCritiera(bestMove.delta, bestMove_tabu.delta)) {
                    bestMove = bestMove_tabu;
                }
#endif
            }
            private void findBestAddOnBlockBorder(ref Move bestMove) { }
            private void findBestChangeOnBlockBorder(ref Move bestMove) { }
            private void findBestRemoveOnBlockBorder(ref Move bestMove) { }
            private void findBestSwapOnBlockBorder(ref Move bestMove) { }
            private void findBestExchangeOnBlockBorder(ref Move bestMove) { }
            private void findBestARLoopOnBlockBorder(ref Move bestMove) { }
            private void findBestARRandOnBlockBorder(ref Move bestMove) { }
            private void findBestARBothOnBlockBorder(ref Move bestMove) { }

            /// <summary> evaluate cost of adding a Assign to nurse without Assign in weekday. </summary>
            private ObjValue tryAddAssign(Weekday weekday, NurseID nurse, Assign a) {
                ObjValue delta = 0;

                // TODO : make sure they won't be the same and leave out this
                if (!a.IsWorking || AssignTable[nurse, weekday].IsWorking) {
                    return DefaultPenalty.ForbiddenMove;
                }

                // hard constraint check
                if (!S.nurses[nurse].skills[a.skill]) { delta += penalty.MissSkill; }

                if (!isValidSuccession(nurse, a.shift, weekday)) { delta += penalty.Succession; }
                if (!isValidPrior(nurse, a.shift, weekday)) { delta += penalty.Succession; }

                if (delta >= DefaultPenalty.MaxObjValue) { return DefaultPenalty.ForbiddenMove; }

                if (W.minNurseNums[weekday, a.shift, a.skill] > assignedNurseNums[weekday, a.shift, a.skill]) {
                    delta -= penalty.Understaff;
                }

                int prevDay = weekday - 1;
                int nextDay = weekday + 1;
                Problem.ScenarioInfo.Contract[] cs = S.contracts;
                ContractID cid = S.nurses[nurse].contract;
                Consecutive c = consecutives[nurse];

                // insufficient staff
                if (missingNurseNums[weekday, a.shift, a.skill] > 0) { delta -= penalty.InsufficientStaff; }

                if (nurseWeights[nurse] == 0) { return delta; }

                // consecutive shift
                Problem.ScenarioInfo.Shift[] s = S.shifts;
                ShiftID sid = a.shift;
                ShiftID prevShiftID = AssignTable[nurse, prevDay].shift;
                if (weekday == Weekdays.Sun) {  // there is no blocks on the right
                    // shiftHigh[weekday] will always be equal to Weekdays.Sun
                    if ((Weekdays.Sun == c.shiftLow[weekday])
                        && (a.shift == prevShiftID)) {
                        delta -= penalty.ConsecutiveShift *
                            Util.distanceToRange(Weekdays.Sun - c.shiftLow[Weekdays.Sat],
                            s[prevShiftID].minConsecutiveShiftNum, s[prevShiftID].maxConsecutiveShiftNum);
                        delta += penalty.ConsecutiveShift * Util.exceedCount(
                            Weekdays.Sun - c.shiftLow[Weekdays.Sat] + 1, s[sid].maxConsecutiveShiftNum);
                    } else {    // have nothing to do with previous block
                        delta += penalty.ConsecutiveShift *    // penalty on day off is counted later
                            Util.exceedCount(1, s[sid].maxConsecutiveShiftNum);
                    }
                } else {
                    ShiftID nextShiftID = AssignTable[nurse, nextDay].shift;
                    if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                        int high = weekday;
                        int low = weekday;
                        if (prevShiftID == a.shift) {
                            low = c.shiftLow[prevDay];
                            delta -= penalty.ConsecutiveShift *
                                Util.distanceToRange(weekday - c.shiftLow[prevDay],
                                s[prevShiftID].minConsecutiveShiftNum, s[prevShiftID].maxConsecutiveShiftNum);
                        }
                        if (nextShiftID == a.shift) {
                            high = c.shiftHigh[nextDay];
                            delta -= penalty.ConsecutiveShift * penaltyDayNum(
                                c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                                s[nextShiftID].minConsecutiveShiftNum, s[nextShiftID].maxConsecutiveShiftNum);
                        }
                        delta += penalty.ConsecutiveShift * penaltyDayNum(high - low + 1, high,
                            s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                    } else if (weekday == c.shiftHigh[weekday]) {
                        if (a.shift == nextShiftID) {
                            int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                            if (consecutiveShiftOfNextBlock >= s[nextShiftID].maxConsecutiveShiftNum) {
                                delta += penalty.ConsecutiveShift;
                            } else if ((c.shiftHigh[nextDay] < Weekdays.Sun)
                                && (consecutiveShiftOfNextBlock < s[nextShiftID].minConsecutiveShiftNum)) {
                                delta -= penalty.ConsecutiveShift;
                            }
                        } else {
                            delta += penalty.ConsecutiveShift * Util.distanceToRange(1,
                                s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                        }
                    } else if (weekday == c.shiftLow[weekday]) {
                        if (a.shift == prevShiftID) {
                            int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                            if (consecutiveShiftOfPrevBlock >= s[prevShiftID].maxConsecutiveShiftNum) {
                                delta += penalty.ConsecutiveShift;
                            } else if (consecutiveShiftOfPrevBlock < s[prevShiftID].minConsecutiveShiftNum) {
                                delta -= penalty.ConsecutiveShift;
                            }
                        } else {
                            delta += penalty.ConsecutiveShift * Util.distanceToRange(1,
                                s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                        }
                    } else {
                        delta += penalty.ConsecutiveShift * Util.distanceToRange(1,
                            s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                    }
                }

                // consecutive day and day-off
                if (weekday == Weekdays.Sun) {  // there is no block on the right
                    // dayHigh[weekday] will always be equal to Weekdays.Sun
                    if (Weekdays.Sun == c.dayLow[weekday]) {
                        delta -= penalty.ConsecutiveDay *
                            Util.distanceToRange(Weekdays.Sun - c.dayLow[Weekdays.Sat],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta -= penalty.ConsecutiveDayOff *
                            Util.exceedCount(1, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDay * Util.exceedCount(
                            Weekdays.Sun - c.dayLow[Weekdays.Sat] + 1, cs[cid].maxConsecutiveDayNum);
                    } else {    // day off block length over 1
                        delta -= penalty.ConsecutiveDayOff * Util.exceedCount(
                            Weekdays.Sun - c.dayLow[Weekdays.Sun] + 1, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDayOff * Util.distanceToRange(Weekdays.Sun - c.dayLow[Weekdays.Sun],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDay * Util.exceedCount(1, cs[cid].maxConsecutiveDayNum);
                    }
                } else {
                    if (c.dayHigh[weekday] == c.dayLow[weekday]) {
                        delta -= penalty.ConsecutiveDay * Util.distanceToRange(weekday - c.dayLow[prevDay],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta -= penalty.ConsecutiveDayOff * Util.distanceToRange(1,
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta -= penalty.ConsecutiveDay * penaltyDayNum(
                            c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDay * penaltyDayNum(
                            c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                    } else if (weekday == c.dayHigh[weekday]) {
                        int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
                        if (consecutiveDayOfNextBlock >= cs[cid].maxConsecutiveDayNum) {
                            delta += penalty.ConsecutiveDay;
                        } else if ((c.dayHigh[nextDay] < Weekdays.Sun)
                            && (consecutiveDayOfNextBlock < cs[cid].minConsecutiveDayNum)) {
                            delta -= penalty.ConsecutiveDay;
                        }
                        int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
                        if (consecutiveDayOfThisBlock > cs[cid].maxConsecutiveDayoffNum) {
                            delta -= penalty.ConsecutiveDayOff;
                        } else if (consecutiveDayOfThisBlock <= cs[cid].minConsecutiveDayoffNum) {
                            delta += penalty.ConsecutiveDayOff;
                        }
                    } else if (weekday == c.dayLow[weekday]) {
                        int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
                        if (consecutiveDayOfPrevBlock >= cs[cid].maxConsecutiveDayNum) {
                            delta += penalty.ConsecutiveDay;
                        } else if (consecutiveDayOfPrevBlock < cs[cid].minConsecutiveDayNum) {
                            delta -= penalty.ConsecutiveDay;
                        }
                        int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
                        if (consecutiveDayOfThisBlock > cs[cid].maxConsecutiveDayoffNum) {
                            delta -= penalty.ConsecutiveDayOff;
                        } else if ((c.dayHigh[weekday] < Weekdays.Sun)
                            && (consecutiveDayOfThisBlock <= cs[cid].minConsecutiveDayoffNum)) {
                            delta += penalty.ConsecutiveDayOff;
                        }
                    } else {
                        delta -= penalty.ConsecutiveDayOff * penaltyDayNum(
                            c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDayOff *
                            Util.distanceToRange(weekday - c.dayLow[weekday],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDay * Util.distanceToRange(1,
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDayOff * penaltyDayNum(
                            c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                    }
                }

                // preference
                if (W.shiftOffs[weekday, a.shift, nurse]) { delta += penalty.Preference; }

                int currentWeek = H.currentWeek;
                if (weekday > Weekdays.Fri) {
                    int theOtherDay = ((weekday == Weekdays.Sat) ? Weekdays.Sun : Weekdays.Sat);
                    // complete weekend
                    if (cs[cid].completeWeekend) {
                        if (AssignTable[nurse, theOtherDay].IsWorking) {
                            delta -= penalty.CompleteWeekend;
                        } else {
                            delta += penalty.CompleteWeekend;
                        }
                    }

                    // total working weekend
                    if (!AssignTable[nurse, theOtherDay].IsWorking) {
#if INRC2_AVERAGE_MAX_WORKING_WEEKEND
                        delta -= penalty.TotalWorkingWeekend * Util.exceedCount(
                            H.totalWorkingWeekendNums[nurse] * S.totalWeekNum,
                            cs[cid].maxWorkingWeekendNum * currentWeek) / S.totalWeekNum;
                        delta += penalty.TotalWorkingWeekend * Util.exceedCount(
                            (H.totalWorkingWeekendNums[nurse] + 1) * S.totalWeekNum,
                            cs[cid].maxWorkingWeekendNum * currentWeek) / S.totalWeekNum;
#else
                        delta -= penalty.TotalWorkingWeekend * Util.exceedCount(0,
                            solver.restMaxWorkingWeekendNum[nurse]) / H.restWeekCount;
                        delta += penalty.TotalWorkingWeekend * Util.exceedCount(H.restWeekCount,
                            solver.restMaxWorkingWeekendNum[nurse]) / H.restWeekCount;
#endif
                    }
                }

                // total assign (expand problem.history.restWeekCount times)
#if INRC2_AVERAGE_TOTAL_SHIFT_NUM
                int totalAssign = (totalAssignNums[nurse] + H.totalAssignNums[nurse]) * S.totalWeekNum;
                delta -= penalty.TotalAssign * Util.distanceToRange(totalAssign,
                    cs[cid].minShiftNum * currentWeek, cs[cid].maxShiftNum * currentWeek) / S.totalWeekNum;
                delta += penalty.TotalAssign * Util.distanceToRange(totalAssign + S.totalWeekNum,
                    cs[cid].minShiftNum * currentWeek, cs[cid].maxShiftNum * currentWeek) / S.totalWeekNum;
#else
                int restMinShift = solver.restMinShiftNum[nurse];
                int restMaxShift = solver.restMaxShiftNum[nurse];
                int totalAssign = totalAssignNums[nurse] * H.restWeekCount;
                delta -= penalty.TotalAssign * Util.distanceToRange(totalAssign,
                    restMinShift, restMaxShift) / H.restWeekCount;
                delta += penalty.TotalAssign * Util.distanceToRange(totalAssign + H.restWeekCount,
                    restMinShift, restMaxShift) / H.restWeekCount;
#endif

                return delta;   // TODO : weight ?
            }
            private ObjValue tryAddAssign(ref Move move) { return tryAddAssign(move.weekday, move.nurse, move.assign); }
            /// <summary> evaluate cost of assigning another Assign or skill to nurse already assigned in weekday. </summary>
            private ObjValue tryChangeAssign(Weekday weekday, NurseID nurse, Assign a) {
                ObjValue delta = 0;

                ShiftID oldShiftID = AssignTable[nurse, weekday].shift;
                SkillID oldSkillID = AssignTable[nurse, weekday].skill;
                // TODO : make sure they won't be the same and leave out this
                if (!a.IsWorking || !ShiftIDs.isWorking(oldShiftID)
                    || ((a.shift == oldShiftID) && (a.skill == oldSkillID))) {
                    return DefaultPenalty.ForbiddenMove;
                }

                if (!S.nurses[nurse].skills[a.skill]) { delta += penalty.MissSkill; }

                if (!isValidSuccession(nurse, a.shift, weekday)) { delta += penalty.Succession; }
                if (!isValidPrior(nurse, a.shift, weekday)) { delta += penalty.Succession; }

                if (W.minNurseNums[weekday, oldShiftID, oldSkillID] >= assignedNurseNums[weekday, oldShiftID, oldSkillID]) {
                    delta += penalty.Understaff;
                }

                if (delta >= DefaultPenalty.MaxObjValue) { return DefaultPenalty.ForbiddenMove; }

                if (!isValidSuccession(nurse, oldShiftID, weekday)) { delta -= penalty.Succession; }
                if (!isValidPrior(nurse, oldShiftID, weekday)) { delta -= penalty.Succession; }

                if (W.minNurseNums[weekday, a.shift, a.skill] > assignedNurseNums[weekday, a.shift, a.skill]) {
                    delta -= penalty.Understaff;
                }

                int prevDay = weekday - 1;
                int nextDay = weekday + 1;
                Consecutive c = consecutives[nurse];

                // insufficient staff
                if (missingNurseNums[weekday, oldShiftID, oldSkillID] >= 0) { delta += penalty.InsufficientStaff; }
                if (missingNurseNums[weekday, a.shift, a.skill] > 0) { delta -= penalty.InsufficientStaff; }

                if (nurseWeights[nurse] == 0) { return delta; }

                if (a.shift != oldShiftID) {
                    // consecutive shift
                    Problem.ScenarioInfo.Shift[] s = S.shifts;
                    ShiftID sid = a.shift;
                    ShiftID prevShiftID = AssignTable[nurse, prevDay].shift;
                    if (weekday == Weekdays.Sun) {  // there is no blocks on the right
                        // shiftHigh[weekday] will always equal to Weekdays.Sun
                        if (Weekdays.Sun == c.shiftLow[weekday]) {
                            if (a.shift == prevShiftID) {
                                delta -= penalty.ConsecutiveShift *
                                    Util.distanceToRange(Weekdays.Sun - c.shiftLow[Weekdays.Sat],
                                    s[prevShiftID].minConsecutiveShiftNum, s[prevShiftID].maxConsecutiveShiftNum);
                                delta -= penalty.ConsecutiveShift *
                                    Util.exceedCount(1, s[oldShiftID].maxConsecutiveShiftNum);
                                delta += penalty.ConsecutiveShift * Util.exceedCount(
                                    Weekdays.Sun - c.shiftLow[Weekdays.Sat] + 1, s[sid].maxConsecutiveShiftNum);
                            } else {
                                delta -= penalty.ConsecutiveShift *
                                    Util.exceedCount(1, s[oldShiftID].maxConsecutiveShiftNum);
                                delta += penalty.ConsecutiveShift *
                                    Util.exceedCount(1, s[sid].maxConsecutiveShiftNum);
                            }
                        } else {    // block length over 1
                            delta -= penalty.ConsecutiveShift * Util.exceedCount(
                                Weekdays.Sun - c.shiftLow[Weekdays.Sun] + 1, s[oldShiftID].maxConsecutiveShiftNum);
                            delta += penalty.ConsecutiveShift * Util.distanceToRange(Weekdays.Sun - c.shiftLow[Weekdays.Sun],
                                s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                            delta += penalty.ConsecutiveShift *
                                Util.exceedCount(1, s[sid].maxConsecutiveShiftNum);
                        }
                    } else {
                        ShiftID nextShiftID = AssignTable[nurse, nextDay].shift;
                        if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                            int high = weekday;
                            int low = weekday;
                            if (prevShiftID == a.shift) {
                                low = c.shiftLow[prevDay];
                                delta -= penalty.ConsecutiveShift *
                                    Util.distanceToRange(weekday - c.shiftLow[prevDay],
                                    s[prevShiftID].minConsecutiveShiftNum, s[prevShiftID].maxConsecutiveShiftNum);
                            }
                            if (nextShiftID == a.shift) {
                                high = c.shiftHigh[nextDay];
                                delta -= penalty.ConsecutiveShift * penaltyDayNum(
                                    c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                                    s[nextShiftID].minConsecutiveShiftNum, s[nextShiftID].maxConsecutiveShiftNum);
                            }
                            delta -= penalty.ConsecutiveShift * Util.distanceToRange(1,
                                s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                            delta += penalty.ConsecutiveShift * penaltyDayNum(high - low + 1, high,
                                s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                        } else if (weekday == c.shiftHigh[weekday]) {
                            if (nextShiftID == a.shift) {
                                int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                                if (consecutiveShiftOfNextBlock >= s[nextShiftID].maxConsecutiveShiftNum) {
                                    delta += penalty.ConsecutiveShift;
                                } else if ((c.shiftHigh[nextDay] < Weekdays.Sun)
                                    && (consecutiveShiftOfNextBlock < s[nextShiftID].minConsecutiveShiftNum)) {
                                    delta -= penalty.ConsecutiveShift;
                                }
                            } else {
                                delta += penalty.ConsecutiveShift * Util.distanceToRange(1,
                                    s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                            }
                            int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
                            if (consecutiveShiftOfThisBlock > s[oldShiftID].maxConsecutiveShiftNum) {
                                delta -= penalty.ConsecutiveShift;
                            } else if (consecutiveShiftOfThisBlock <= s[oldShiftID].minConsecutiveShiftNum) {
                                delta += penalty.ConsecutiveShift;
                            }
                        } else if (weekday == c.shiftLow[weekday]) {
                            if (prevShiftID == a.shift) {
                                int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                                if (consecutiveShiftOfPrevBlock >= s[prevShiftID].maxConsecutiveShiftNum) {
                                    delta += penalty.ConsecutiveShift;
                                } else if (consecutiveShiftOfPrevBlock < s[prevShiftID].minConsecutiveShiftNum) {
                                    delta -= penalty.ConsecutiveShift;
                                }
                            } else {
                                delta += penalty.ConsecutiveShift * Util.distanceToRange(1,
                                    s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                            }
                            int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
                            if (consecutiveShiftOfThisBlock > s[oldShiftID].maxConsecutiveShiftNum) {
                                delta -= penalty.ConsecutiveShift;
                            } else if ((c.shiftHigh[weekday] < Weekdays.Sun)
                                && (consecutiveShiftOfThisBlock <= s[oldShiftID].minConsecutiveShiftNum)) {
                                delta += penalty.ConsecutiveShift;
                            }
                        } else {
                            delta -= penalty.ConsecutiveShift * penaltyDayNum(
                                c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                                s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                            delta += penalty.ConsecutiveShift *
                                Util.distanceToRange(weekday - c.shiftLow[weekday],
                                s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                            delta += penalty.ConsecutiveShift * Util.distanceToRange(1,
                                s[sid].minConsecutiveShiftNum, s[sid].maxConsecutiveShiftNum);
                            delta += penalty.ConsecutiveShift *
                                penaltyDayNum(c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                                s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                        }
                    }

                    // preference
                    if (W.shiftOffs[weekday, a.shift, nurse]) { delta += penalty.Preference; }
                    if (W.shiftOffs[weekday, oldShiftID, nurse]) { delta -= penalty.Preference; }
                }

                return delta;   // TODO : weight ?
            }
            private ObjValue tryChangeAssign(ref Move move) { return tryChangeAssign(move.weekday, move.nurse, move.assign); }
            /// <summary> evaluate cost of removing the Assign from nurse already assigned in weekday. </summary>
            private ObjValue tryRemoveAssign(Weekday weekday, NurseID nurse) {
                ObjValue delta = 0;

                ShiftID oldShiftID = AssignTable[nurse, weekday].shift;
                SkillID oldSkillID = AssignTable[nurse, weekday].skill;
                // TODO : make sure they won't be the same and leave out this
                if (!ShiftIDs.isWorking(oldShiftID)) {
                    return DefaultPenalty.ForbiddenMove;
                }

                if (W.minNurseNums[weekday, oldShiftID, oldSkillID] >=
                    assignedNurseNums[weekday, oldShiftID, oldSkillID]) {
                    delta += penalty.Understaff;
                }

                if (delta >= DefaultPenalty.MaxObjValue) { return DefaultPenalty.ForbiddenMove; }

                if (!isValidSuccession(nurse, oldShiftID, weekday)) { delta -= penalty.Succession; }
                if (!isValidPrior(nurse, oldShiftID, weekday)) { delta -= penalty.Succession; }

                int prevDay = weekday - 1;
                int nextDay = weekday + 1;
                Problem.ScenarioInfo.Contract[] cs = S.contracts;
                ContractID cid = S.nurses[nurse].contract;
                Consecutive c = consecutives[nurse];

                // insufficient staff
                if (missingNurseNums[weekday, oldShiftID, oldSkillID] >= 0) {
                    delta += penalty.InsufficientStaff;
                }

                if (nurseWeights[nurse] == 0) { return delta; }

                // consecutive shift
                Problem.ScenarioInfo.Shift[] s = S.shifts;
                if (weekday == Weekdays.Sun) {  // there is no block on the right
                    if (Weekdays.Sun == c.shiftLow[weekday]) {
                        delta -= penalty.ConsecutiveShift * Util.exceedCount(1, s[oldShiftID].maxConsecutiveShiftNum);
                    } else {
                        delta -= penalty.ConsecutiveShift * Util.exceedCount(
                            Weekdays.Sun - c.shiftLow[weekday] + 1, s[oldShiftID].maxConsecutiveShiftNum);
                        delta += penalty.ConsecutiveShift * Util.distanceToRange(Weekdays.Sun - c.shiftLow[weekday],
                            s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                    }
                } else {
                    if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                        delta -= penalty.ConsecutiveShift * Util.distanceToRange(1,
                            s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                    } else if (weekday == c.shiftHigh[weekday]) {
                        int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
                        if (consecutiveShiftOfThisBlock > s[oldShiftID].maxConsecutiveShiftNum) {
                            delta -= penalty.ConsecutiveShift;
                        } else if (consecutiveShiftOfThisBlock <= s[oldShiftID].minConsecutiveShiftNum) {
                            delta += penalty.ConsecutiveShift;
                        }
                    } else if (weekday == c.shiftLow[weekday]) {
                        int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
                        if (consecutiveShiftOfThisBlock > s[oldShiftID].maxConsecutiveShiftNum) {
                            delta -= penalty.ConsecutiveShift;
                        } else if ((c.shiftHigh[weekday] < Weekdays.Sun)
                            && (consecutiveShiftOfThisBlock <= s[oldShiftID].minConsecutiveShiftNum)) {
                            delta += penalty.ConsecutiveShift;
                        }
                    } else {
                        delta -= penalty.ConsecutiveShift * penaltyDayNum(
                            c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                            s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                        delta += penalty.ConsecutiveShift * Util.distanceToRange(weekday - c.shiftLow[weekday],
                            s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                        delta += penalty.ConsecutiveShift * penaltyDayNum(
                            c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                            s[oldShiftID].minConsecutiveShiftNum, s[oldShiftID].maxConsecutiveShiftNum);
                    }
                }

                // consecutive day and day-off
                if (weekday == Weekdays.Sun) {  // there is no blocks on the right
                    // dayHigh[weekday] will always be equal to Weekdays.Sun
                    if (Weekdays.Sun == c.dayLow[weekday]) {
                        delta -= penalty.ConsecutiveDayOff *
                            Util.distanceToRange(Weekdays.Sun - c.dayLow[Weekdays.Sat],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta -= penalty.ConsecutiveDay *
                            Util.exceedCount(1, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDayOff * Util.exceedCount(
                            Weekdays.Sun - c.dayLow[Weekdays.Sat] + 1, cs[cid].maxConsecutiveDayoffNum);
                    } else {    // day off block length over 1
                        delta -= penalty.ConsecutiveDay * Util.exceedCount(
                            Weekdays.Sun - c.dayLow[Weekdays.Sun] + 1, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDay * Util.distanceToRange(Weekdays.Sun - c.dayLow[Weekdays.Sun],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDayOff * Util.exceedCount(1, cs[cid].maxConsecutiveDayoffNum);
                    }
                } else {
                    if (c.dayHigh[weekday] == c.dayLow[weekday]) {
                        delta -= penalty.ConsecutiveDayOff *
                            Util.distanceToRange(weekday - c.dayLow[prevDay],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta -= penalty.ConsecutiveDay *
                            Util.distanceToRange(1,
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta -= penalty.ConsecutiveDayOff * penaltyDayNum(
                            c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDayOff * penaltyDayNum(
                            c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                    } else if (weekday == c.dayHigh[weekday]) {
                        int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
                        if (consecutiveDayOfNextBlock >= cs[cid].maxConsecutiveDayoffNum) {
                            delta += penalty.ConsecutiveDayOff;
                        } else if ((c.dayHigh[nextDay] < Weekdays.Sun)
                            && (consecutiveDayOfNextBlock < cs[cid].minConsecutiveDayoffNum)) {
                            delta -= penalty.ConsecutiveDayOff;
                        }
                        int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
                        if (consecutiveDayOfThisBlock > cs[cid].maxConsecutiveDayNum) {
                            delta -= penalty.ConsecutiveDay;
                        } else if (consecutiveDayOfThisBlock <= cs[cid].minConsecutiveDayNum) {
                            delta += penalty.ConsecutiveDay;
                        }
                    } else if (weekday == c.dayLow[weekday]) {
                        int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
                        if (consecutiveDayOfPrevBlock >= cs[cid].maxConsecutiveDayoffNum) {
                            delta += penalty.ConsecutiveDayOff;
                        } else if (consecutiveDayOfPrevBlock < cs[cid].minConsecutiveDayoffNum) {
                            delta -= penalty.ConsecutiveDayOff;
                        }
                        int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
                        if (consecutiveDayOfThisBlock > cs[cid].maxConsecutiveDayNum) {
                            delta -= penalty.ConsecutiveDay;
                        } else if ((c.dayHigh[weekday] < Weekdays.Sun)
                            && (consecutiveDayOfThisBlock <= cs[cid].minConsecutiveDayNum)) {
                            delta += penalty.ConsecutiveDay;
                        }
                    } else {
                        delta -= penalty.ConsecutiveDay * penaltyDayNum(
                            c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDay *
                            Util.distanceToRange(weekday - c.dayLow[weekday],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                        delta += penalty.ConsecutiveDayOff * Util.distanceToRange(1,
                            cs[cid].minConsecutiveDayoffNum, cs[cid].maxConsecutiveDayoffNum);
                        delta += penalty.ConsecutiveDay *
                            penaltyDayNum(c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                            cs[cid].minConsecutiveDayNum, cs[cid].maxConsecutiveDayNum);
                    }
                }

                // preference
                if (W.shiftOffs[weekday, oldShiftID, nurse]) { delta -= penalty.Preference; }

                int currentWeek = H.currentWeek;
                if (weekday > Weekdays.Fri) {
                    int theOtherDay = ((weekday == Weekdays.Sat) ? Weekdays.Sun : Weekdays.Sat);
                    // complete weekend
                    if (cs[cid].completeWeekend) {
                        if (AssignTable[nurse, theOtherDay].IsWorking) {
                            delta += penalty.CompleteWeekend;
                        } else {
                            delta -= penalty.CompleteWeekend;
                        }
                    }

                    // total working weekend
                    if (!AssignTable[nurse, theOtherDay].IsWorking) {
#if INRC2_AVERAGE_MAX_WORKING_WEEKEND
                        delta -= penalty.TotalWorkingWeekend * Util.exceedCount(
                            (H.totalWorkingWeekendNums[nurse] + 1) * S.totalWeekNum,
                            cs[cid].maxWorkingWeekendNum * currentWeek) / S.totalWeekNum;
                        delta += penalty.TotalWorkingWeekend * Util.exceedCount(
                            H.totalWorkingWeekendNums[nurse] * S.totalWeekNum,
                            cs[cid].maxWorkingWeekendNum * currentWeek) / S.totalWeekNum;
#else
                        delta -= penalty.TotalWorkingWeekend * Util.exceedCount(H.restWeekCount,
                            solver.restMaxWorkingWeekendNum[nurse]) / H.restWeekCount;
                        delta += penalty.TotalWorkingWeekend * Util.exceedCount(0,
                            solver.restMaxWorkingWeekendNum[nurse]) / H.restWeekCount;
#endif
                    }
                }

                // total assign (expand problem.history.restWeekCount times)
#if INRC2_AVERAGE_TOTAL_SHIFT_NUM
                int totalAssign = (totalAssignNums[nurse] + H.totalAssignNums[nurse]) * S.totalWeekNum;
                delta -= penalty.TotalAssign * Util.distanceToRange(totalAssign,
                    cs[cid].minShiftNum * currentWeek, cs[cid].maxShiftNum * currentWeek) / S.totalWeekNum;
                delta += penalty.TotalAssign * Util.distanceToRange(totalAssign - S.totalWeekNum,
                    cs[cid].minShiftNum * currentWeek, cs[cid].maxShiftNum * currentWeek) / S.totalWeekNum;
#else
                int restMinShift = solver.restMinShiftNum[nurse];
                int restMaxShift = solver.restMaxShiftNum[nurse];
                int totalAssign = totalAssignNums[nurse] * H.restWeekCount;
                delta -= penalty.TotalAssign * Util.distanceToRange(totalAssign,
                    restMinShift, restMaxShift) / H.restWeekCount;
                delta += penalty.TotalAssign * Util.distanceToRange(totalAssign - H.restWeekCount,
                    restMinShift, restMaxShift) / H.restWeekCount;
#endif

                return delta;   // TODO : weight ?
            }
            private ObjValue tryRemoveAssign(ref Move move) { return tryRemoveAssign(move.weekday, move.nurse); }
            /// <summary> evaluate cost of swapping Assign of two nurses in the same day. </summary>
            private ObjValue trySwapNurse(Weekday weekday, NurseID nurse, NurseID nurse2) {
                // TODO : make sure they won't be the same and leave out this
                if (nurse == nurse2) { return DefaultPenalty.ForbiddenMove; }

                ObjValue delta;
                if (AssignTable[nurse, weekday].IsWorking) {
                    if (AssignTable[nurse2, weekday].IsWorking) {
                        delta = tryChangeAssign(weekday, nurse, AssignTable[nurse2, weekday]);
                        return ((delta >= DefaultPenalty.MaxObjValue)
                            ? DefaultPenalty.ForbiddenMove
                            : (delta + tryChangeAssign(weekday, nurse2, AssignTable[nurse, weekday])));
                    } else {
                        delta = tryRemoveAssign(weekday, nurse);
                        return ((delta >= DefaultPenalty.MaxObjValue)
                            ? DefaultPenalty.ForbiddenMove
                            : (delta + tryAddAssign(weekday, nurse2, AssignTable[nurse, weekday])));
                    }
                } else {
                    if (AssignTable[nurse2, weekday].IsWorking) {
                        delta = tryAddAssign(weekday, nurse, AssignTable[nurse2, weekday]);
                        return ((delta >= DefaultPenalty.MaxObjValue)
                            ? DefaultPenalty.ForbiddenMove
                            : (delta + tryRemoveAssign(weekday, nurse2)));
                    } else {    // no change
                        return DefaultPenalty.ForbiddenMove;
                    }
                }
            }
            private ObjValue trySwapNurse(ref Move move) {
                penalty.setSwapMode();
                ObjValue delta = trySwapNurse(move.weekday, move.nurse, move.nurse2);
                penalty.recoverLastMode();

                return delta;
            }
            /// <summary>
            // evaluate cost of swapping Assign of two nurses in consecutive days start from weekday
            // and record the selected end of the block into weekday2
            // the recorded move will always be no tabu move or meet aspiration criteria
            /// </summary>
            private ObjValue trySwapBlock(Weekday weekday, ref Weekday weekday2, NurseID nurse, NurseID nurse2) {
                // TODO : make sure they won't be the same and leave out this
                if (nurse == nurse2) {
                    return DefaultPenalty.ForbiddenMove;
                }

                if (!(isValidSuccession(nurse, AssignTable[nurse2, weekday].shift, weekday)
                    && isValidSuccession(nurse2, AssignTable[nurse, weekday].shift, weekday))) {
                    return DefaultPenalty.ForbiddenMove;
                }

                Util.RandSelect rs = new Util.RandSelect();
                ObjValue delta = 0;
                ObjValue minDelta = DefaultPenalty.ForbiddenMove;
#if !INRC2_BLOCK_SWAP_NO_TABU
                Util.RandSelect rs_tabu = new Util.RandSelect();
                ObjValue minDelta_tabu = DefaultPenalty.ForbiddenMove;
                Weekday weekday2_tabu = weekday;

                int count = 0;
                int noTabuCount = 0;
#endif
                // try each block length
                Weekday w = weekday;
                while (S.nurses[nurse].skills[AssignTable[nurse2, w].skill]
                    && S.nurses[nurse2].skills[AssignTable[nurse, w].skill]) {
                    // longer blocks will also miss this skill
                    delta += trySwapNurse(w, nurse, nurse2);

#if !INRC2_BLOCK_SWAP_NO_TABU
                    count++;
                    if (noSwapTabu(w, nurse, nurse2)) { noTabuCount++; }
#endif

                    if (delta < DefaultPenalty.MaxObjValue) {
                        if (isValidPrior(nurse, AssignTable[nurse2, w].shift, w)
                            && isValidPrior(nurse2, AssignTable[nurse, w].shift, w)) {
#if !INRC2_BLOCK_SWAP_NO_TABU
                            if (noBlockSwapTabu(noTabuCount, count)) {
#endif
                                if (rs.isMinimal(delta, minDelta, solver.rand.Next())) {
                                    minDelta = delta;
                                    weekday2 = w;
                                }
#if !INRC2_BLOCK_SWAP_NO_TABU
                            } else {    // tabu
                                if (rs_tabu.isMinimal(delta, minDelta_tabu, solver.rand.Next())) {
                                    minDelta_tabu = delta;
                                    weekday2_tabu = w;
                                }
                            }
#endif
                        }
                    } else {    // two day off
                        delta -= DefaultPenalty.ForbiddenMove;
                    }

                    if (w >= Weekdays.Sun) { break; }

                    swapNurse(w, nurse, nurse2);
                    w++;
                }

#if !INRC2_BLOCK_SWAP_NO_TABU
                if (aspirationCritiera(minDelta, minDelta_tabu)) {
                    minDelta = minDelta_tabu;
                    weekday2 = weekday2_tabu;
                }
#endif

                // recover original data
                while ((--w) >= weekday) {
                    swapNurse(w, nurse, nurse2);
                }

                return minDelta;
            }
            private ObjValue trySwapBlock(ref Move move) {
                penalty.setBlockSwapMode();
                ObjValue delta = trySwapBlock(move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                penalty.recoverLastMode();

                return delta;
            }
            /// <summary>
            // evaluate cost of swapping Assign of two nurses in consecutive days in a week
            // and record the block information into weekday and weekday2
            // the recorded move will always be no tabu move or meet aspiration criteria. 
            /// </summary>
            private ObjValue trySwapBlock_Fast(ref Weekday weekday, ref Weekday weekday2, NurseID nurse, NurseID nurse2) {
                // TODO : make sure they won't be the same and leave out this
                if (nurse == nurse2) {
                    return DefaultPenalty.ForbiddenMove;
                }

                Util.RandSelect rs = new Util.RandSelect();
                ObjValue delta = 0;
                ObjValue minDelta = DefaultPenalty.ForbiddenMove;

                // prepare for hard constraint check and tabu judgment
                bool[] hasSkill = new bool[Weekdays.Length];
                for (Weekday w = Weekdays.Mon; w <= Weekdays.Sun; w++) {
                    hasSkill[w] = (S.nurses[nurse].skills[AssignTable[nurse2, w].skill]
                        && S.nurses[nurse2].skills[AssignTable[nurse, w].skill]);
                }

                weekday = Weekdays.Mon;
                weekday2 = Weekdays.Mon;

#if !INRC2_BLOCK_SWAP_NO_TABU
                Util.RandSelect rs_tabu = new Util.RandSelect();
                ObjValue minDelta_tabu = DefaultPenalty.ForbiddenMove;

                int[,] noTabuCount = new int[Weekdays.Length, Weekdays.Length];
                bool[,] noTabu = new bool[Weekdays.Length, Weekdays.Length];
                for (Weekday w = Weekdays.Mon; w <= Weekdays.Sun; w++) {
                    noTabuCount[w, w] = (noSwapTabu(w, nurse, nurse2) ? 1 : 0);
                    noTabu[w, w] = (noTabuCount[w, w] > 0);
                }
                for (Weekday w = Weekdays.Mon; w <= Weekdays.Sun; w++) {
                    for (Weekday w2 = w + 1; w2 <= Weekdays.Sun; w2++) {
                        noTabuCount[w, w2] = noTabuCount[w, w2 - 1] + noTabuCount[w2, w2];
                        noTabu[w, w2] = noBlockSwapTabu(noTabuCount[w, w2], w2 - w + 1);
                    }
                }

                Weekday weekday_tabu = weekday;
                Weekday weekday2_tabu = weekday2;
#endif

                // try each block length
                for (Weekday w = Weekdays.Mon, w2; w <= Weekdays.Sun; w++) {
                    if (!(isValidSuccession(nurse, AssignTable[nurse2, w].shift, w)
                        && isValidSuccession(nurse2, AssignTable[nurse, w].shift, w))) {
                        continue;
                    }

                    w2 = w;
                    for (; (w2 <= Weekdays.Sun) && hasSkill[w2]; w2++) { // longer blocks will also miss this skill
                        delta += trySwapNurse(w2, nurse, nurse2);

                        if (delta < DefaultPenalty.MaxObjValue) {
                            swapNurse(w2, nurse, nurse2);

                            if (isValidPrior(nurse, AssignTable[nurse, w2].shift, w2)
                                && isValidPrior(nurse2, AssignTable[nurse2, w2].shift, w2)) {
#if !INRC2_BLOCK_SWAP_NO_TABU
                                if (noTabu[w, w2]) {
#endif
                                    if (rs.isMinimal(delta, minDelta, solver.rand.Next())) {
                                        minDelta = delta;
                                        weekday = w;
                                        weekday2 = w2;
                                    }
#if !INRC2_BLOCK_SWAP_NO_TABU
                                } else {    // tabu
                                    if (rs_tabu.isMinimal(delta, minDelta_tabu, solver.rand.Next())) {
                                        minDelta_tabu = delta;
                                        weekday_tabu = w;
                                        weekday2_tabu = w2;
                                    }
                                }
#endif
                            }
                        } else {    // two day off
                            delta -= DefaultPenalty.ForbiddenMove;
                        }
                    }

                    if (w == w2) { continue; }  // the first day is not swapped

                    do {
                        delta += trySwapNurse(w, nurse, nurse2);
                        if (delta < DefaultPenalty.MaxObjValue) {
                            swapNurse(w, nurse, nurse2);
                        } else {    // two day off
                            delta -= DefaultPenalty.ForbiddenMove;
                        }
                        w++;
                    } while ((w < w2)
                        && !(isValidSuccession(nurse, AssignTable[nurse, w].shift, w)
                        && isValidSuccession(nurse2, AssignTable[nurse2, w].shift, w)));

                    while (w < (w2--)) {
                        if (isValidPrior(nurse, AssignTable[nurse, w2].shift, w2)
                            && isValidPrior(nurse2, AssignTable[nurse2, w2].shift, w2)) {
#if !INRC2_BLOCK_SWAP_NO_TABU
                            if (noTabu[w, w2]) {
#endif
                                if (rs.isMinimal(delta, minDelta, solver.rand.Next())) {
                                    minDelta = delta;
                                    weekday = w;
                                    weekday2 = w2;
                                }
#if !INRC2_BLOCK_SWAP_NO_TABU
                            } else {    // tabu
                                if (rs_tabu.isMinimal(delta, minDelta_tabu, solver.rand.Next())) {
                                    minDelta_tabu = delta;
                                    weekday_tabu = w;
                                    weekday2_tabu = w2;
                                }
                            }
#endif
                        }

                        delta += trySwapNurse(w2, nurse, nurse2);
                        if (delta < DefaultPenalty.MaxObjValue) {
                            swapNurse(w2, nurse, nurse2);
                        } else {    // two day off
                            delta -= DefaultPenalty.ForbiddenMove;
                        }
                    }
                }

#if !INRC2_BLOCK_SWAP_NO_TABU
                if (aspirationCritiera(minDelta, minDelta_tabu)) {
                    minDelta = minDelta_tabu;
                    weekday = weekday_tabu;
                    weekday2 = weekday2_tabu;
                }
#endif
                return minDelta;
            }
            private ObjValue trySwapBlock_Fast(ref Move move) {
                penalty.setBlockSwapMode();
                ObjValue delta = trySwapBlock_Fast(ref move.weekday, ref move.weekday2, move.nurse, move.nurse2);
                penalty.recoverLastMode();

                return delta;
            }
            /// <summary> evaluate cost of exchanging Assign of a nurse on two days. </summary>
            private ObjValue tryExchangeDay(Weekday weekday, NurseID nurse, Weekday weekday2) {
                // TODO : make sure they won't be the same and leave out this
                if (weekday == weekday2) {
                    return DefaultPenalty.ForbiddenMove;
                }

                // both are day off will change nothing
                if (!AssignTable[nurse, weekday].IsWorking && !AssignTable[nurse, weekday2].IsWorking) {
                    return DefaultPenalty.ForbiddenMove;
                }

                // check succession 
                ShiftID shift = AssignTable[nurse, weekday].shift;
                ShiftID shift2 = AssignTable[nurse, weekday2].shift;
                if (weekday == weekday2 + 1) {
                    if (!(isValidSuccession(nurse, shift, weekday2)
                        && S.shifts[shift].legalNextShifts[shift2]
                        && isValidPrior(nurse, shift2, weekday))) {
                        return DefaultPenalty.ForbiddenMove;
                    }
                } else if (weekday == weekday2 - 1) {
                    if (!(isValidSuccession(nurse, shift2, weekday)
                        && S.shifts[shift2].legalNextShifts[shift]
                        && isValidPrior(nurse, shift, weekday2))) {
                        return DefaultPenalty.ForbiddenMove;
                    }
                } else {
                    if (!(isValidSuccession(nurse, shift, weekday2)
                        && isValidPrior(nurse, shift, weekday2)
                        && isValidSuccession(nurse, shift2, weekday)
                        && isValidPrior(nurse, shift2, weekday))) {
                        return DefaultPenalty.ForbiddenMove;
                    }
                }

                ObjValue delta = 0;

                if (AssignTable[nurse, weekday].IsWorking) {
                    if (AssignTable[nurse, weekday2].IsWorking) {
                        delta += tryChangeAssign(weekday, nurse, AssignTable[nurse, weekday2]);
                        if (delta < DefaultPenalty.MaxObjValue) {
                            changeAssign(weekday, nurse, AssignTable[nurse, weekday2]);
                            delta += tryChangeAssign(weekday2, nurse, AssignTable[nurse, weekday]);
                            changeAssign(weekday, nurse, AssignTable[nurse, weekday]);
                        }
                    } else {
                        delta += tryRemoveAssign(weekday, nurse);
                        if (delta < DefaultPenalty.MaxObjValue) {
                            removeAssign(weekday, nurse);
                            delta += tryAddAssign(weekday2, nurse, AssignTable[nurse, weekday]);
                            addAssign(weekday, nurse, AssignTable[nurse, weekday]);
                        }
                    }
                } else {
                    delta += tryAddAssign(weekday, nurse, AssignTable[nurse, weekday2]);
                    if (delta < DefaultPenalty.MaxObjValue) {
                        addAssign(weekday, nurse, AssignTable[nurse, weekday2]);
                        delta += tryRemoveAssign(weekday2, nurse);
                        removeAssign(weekday, nurse);
                    }
                }

                return delta;
            }
            private ObjValue tryExchangeDay(ref Move move) {
                penalty.setExchangeMode();
                ObjValue delta = tryExchangeDay(move.weekday, move.nurse, move.weekday2);
                penalty.recoverLastMode();

                return delta;
            }

            /// <summary> apply assigning a Assign to nurse without Assign in weekday. </summary>
            private void addAssign(Weekday weekday, NurseID nurse, Assign a) {
                updateConsecutive(weekday, nurse, a.shift);

                missingNurseNums[weekday, a.shift, a.skill]--;

                totalAssignNums[nurse]++;

                AssignTable[nurse, weekday] = a;
            }
            private void addAssign(ref Move move) { addAssign(move.weekday, move.nurse, move.assign); }
            /// <summary> apply assigning another Assign or skill to nurse already assigned in weekday. </summary>
            private void changeAssign(Weekday weekday, NurseID nurse, Assign a) {
                if (a.shift != AssignTable[nurse, weekday].shift) {  // for just change skill
                    updateConsecutive(weekday, nurse, a.shift);
                }

                missingNurseNums[weekday, a.shift, a.skill]--;
                missingNurseNums[weekday, AssignTable[nurse, weekday].shift, AssignTable[nurse, weekday].skill]++;

                AssignTable[nurse, weekday] = a;
            }
            private void changeAssign(ref Move move) { changeAssign(move.weekday, move.nurse, move.assign); }
            /// <summary> apply removing a Assign to nurse in weekday. </summary>
            private void removeAssign(Weekday weekday, NurseID nurse) {
                updateConsecutive(weekday, nurse, ShiftIDs.None);

                missingNurseNums[weekday, AssignTable[nurse, weekday].shift, AssignTable[nurse, weekday].skill]++;

                totalAssignNums[nurse]--;

                AssignTable[nurse, weekday].shift = ShiftIDs.None;
            }
            private void removeAssign(ref Move move) { removeAssign(move.weekday, move.nurse); }
            /// <summary> apply swapping Assign of two nurses in the same day. </summary>
            private void swapNurse(Weekday weekday, NurseID nurse, NurseID nurse2) {
                if (AssignTable[nurse, weekday].IsWorking) {
                    if (AssignTable[nurse2, weekday].IsWorking) {
                        Assign a = AssignTable[nurse, weekday];
                        changeAssign(weekday, nurse, AssignTable[nurse2, weekday]);
                        changeAssign(weekday, nurse2, a);
                    } else {
                        addAssign(weekday, nurse2, AssignTable[nurse, weekday]);
                        removeAssign(weekday, nurse);
                    }
                } else {
                    if (AssignTable[nurse2, weekday].IsWorking) {
                        addAssign(weekday, nurse, AssignTable[nurse2, weekday]);
                        removeAssign(weekday, nurse2);
                    }
                }
            }
            private void swapNurse(ref Move move) { swapNurse(move.weekday, move.nurse, move.nurse2); }
            /// <summary> apply swapping Assign of two nurses in consecutive days within [weekday, weekday2]. </summary>
            private void swapBlock(Weekday weekday, Weekday weekday2, NurseID nurse, NurseID nurse2) {
                for (; weekday <= weekday2; ++weekday) {
                    swapNurse(weekday, nurse, nurse2);
                }
            }
            private void swapBlock(ref Move move) { swapBlock(move.weekday, move.weekday2, move.nurse, move.nurse2); }
            /// <summary> apply exchanging Assign of a nurse on two days. </summary>
            private void exchangeDay(Weekday weekday, NurseID nurse, Weekday weekday2) {
                if (AssignTable[nurse, weekday].IsWorking) {
                    if (AssignTable[nurse, weekday2].IsWorking) {
                        Assign a = AssignTable[nurse, weekday];
                        changeAssign(weekday, nurse, AssignTable[nurse, weekday2]);
                        changeAssign(weekday2, nurse, a);
                    } else {
                        addAssign(weekday2, nurse, AssignTable[nurse, weekday]);
                        removeAssign(weekday, nurse);
                    }
                } else {
                    if (AssignTable[nurse, weekday2].IsWorking) {
                        addAssign(weekday, nurse, AssignTable[nurse, weekday2]);
                        removeAssign(weekday2, nurse);
                    }
                }
            }
            private void exchangeDay(ref Move move) { exchangeDay(move.weekday, move.nurse, move.weekday2); }

            /// <summary> handle consecutive block change. </summary>
            private void updateConsecutive(Weekday weekday, NurseID nurse, ShiftID shift) {
                Consecutive c = consecutives[nurse];
                int nextDay = weekday + 1;
                int prevDay = weekday - 1;

                // consider day
                bool isDayHigh = (weekday == c.dayHigh[weekday]);
                bool isDayLow = (weekday == c.dayLow[weekday]);
                if (AssignTable[nurse, weekday].IsWorking != ShiftIDs.isWorking(shift)) {
                    if (isDayHigh && isDayLow) {
                        assignSingle(weekday, c.dayHigh, c.dayLow, (weekday != Weekdays.Sun), true);
                    } else if (isDayHigh) {
                        assignHigh(weekday, c.dayHigh, c.dayLow, (weekday != Weekdays.Sun));
                    } else if (isDayLow) {
                        assignLow(weekday, c.dayHigh, c.dayLow, true);
                    } else {
                        assignMiddle(weekday, c.dayHigh, c.dayLow);
                    }
                }

                // consider shift
                bool isShiftHigh = (weekday == c.shiftHigh[weekday]);
                bool isShiftLow = (weekday == c.shiftLow[weekday]);
                if (isShiftHigh && isShiftLow) {
                    assignSingle(weekday, c.shiftHigh, c.shiftLow,
                        ((nextDay <= Weekdays.Sun) && (shift == AssignTable[nurse, nextDay].shift)),
                        (shift == AssignTable[nurse, prevDay].shift));
                } else if (isShiftHigh) {
                    assignHigh(weekday, c.shiftHigh, c.shiftLow,
                        ((nextDay <= Weekdays.Sun) && (shift == AssignTable[nurse, nextDay].shift)));
                } else if (isShiftLow) {
                    assignLow(weekday, c.shiftHigh, c.shiftLow,
                        (shift == AssignTable[nurse, prevDay].shift));
                } else {
                    assignMiddle(weekday, c.shiftHigh, c.shiftLow);
                }
            }
            /// <summary> the assignment is on the right side of a consecutive block. </summary>
            private void assignHigh(Weekday weekday, Weekday[] high, Weekday[] low, bool affectRight) {
                int nextDay = weekday + 1;
                int prevDay = weekday - 1;
                for (int d = prevDay; (d >= Weekdays.LastWeek) && (d >= low[weekday]); --d) {
                    high[d] = prevDay;
                }
                if (affectRight) {
                    for (int d = nextDay; d <= high[nextDay]; ++d) {
                        low[d] = weekday;
                    }
                    high[weekday] = high[nextDay];
                } else {
                    high[weekday] = weekday;
                }
                low[weekday] = weekday;
            }
            /// <summary> the assignment is on the left side of a consecutive block. </summary>
            private void assignLow(Weekday weekday, Weekday[] high, Weekday[] low, bool affectLeft) {
                int nextDay = weekday + 1;
                int prevDay = weekday - 1;
                for (int d = nextDay; d <= high[weekday]; ++d) {
                    low[d] = nextDay;
                }
                if (affectLeft) {
                    for (int d = prevDay; (d >= Weekdays.LastWeek) && (d >= low[prevDay]); --d) {
                        high[d] = weekday;
                    }
                    low[weekday] = low[prevDay];
                } else {
                    low[weekday] = weekday;
                }
                high[weekday] = weekday;
            }
            /// <summary> the assignment is in the middle of a consecutive block. </summary>
            private void assignMiddle(Weekday weekday, Weekday[] high, Weekday[] low) {
                int nextDay = weekday + 1;
                int prevDay = weekday - 1;
                for (int d = nextDay; d <= high[weekday]; ++d) {
                    low[d] = nextDay;
                }
                for (int d = prevDay; (d >= Weekdays.LastWeek) && (d >= low[weekday]); --d) {
                    high[d] = prevDay;
                }
                high[weekday] = weekday;
                low[weekday] = weekday;
            }
            /// <summary> the assignment is on a consecutive block with single slot. </summary>
            private void assignSingle(Weekday weekday, Weekday[] high, Weekday[] low, bool affectRight, bool affectLeft) {
                int nextDay = weekday + 1;
                int prevDay = weekday - 1;
                int h = affectRight ? high[nextDay] : weekday;
                int l = affectLeft ? low[prevDay] : weekday;
                if (affectRight) {
                    for (int d = nextDay; d <= high[nextDay]; ++d) {
                        low[d] = l;
                    }
                    high[weekday] = h;
                }
                if (affectLeft) {
                    for (int d = prevDay; (d >= Weekdays.LastWeek) && (d >= low[prevDay]); --d) {
                        high[d] = h;
                    }
                    low[weekday] = l;
                }
            }

#if INRC2_USE_TABU
            bool noAddTabu(ref Move move) {
                return (iterCount > shiftTabu[move.nurse, move.weekday, move.assign.shift, move.assign.skill]);
            }
            bool noChangeTabu(ref Move move) {
                return (iterCount > shiftTabu[move.nurse, move.weekday, move.assign.shift, move.assign.skill]);
            }
            bool noRemoveTabu(ref Move move) {
                return (iterCount > dayTabu[move.nurse, move.weekday]);
            }
            bool noSwapTabu(Weekday weekday, NurseID nurse, NurseID nurse2) {
                Assign a = AssignTable[nurse, weekday];
                Assign a2 = AssignTable[nurse2, weekday];

                if (a.IsWorking) {
                    if (a2.IsWorking) {
                        return ((iterCount > shiftTabu[nurse, weekday, a2.shift, a2.skill])
                            || (iterCount > shiftTabu[nurse2, weekday, a.shift, a.skill]));
                    } else {
                        return ((iterCount > dayTabu[nurse, weekday])
                            || (iterCount > shiftTabu[nurse2, weekday, a.shift, a.skill]));
                    }
                } else {
                    if (a2.IsWorking) {
                        return ((iterCount > shiftTabu[nurse, weekday, a2.shift, a2.skill])
                            || (iterCount > dayTabu[nurse2, weekday]));
                    } else {    // no change
                        return true;
                    }
                }
            }
            bool noSwapTabu(ref Move move) {
                return noSwapTabu(move.weekday, move.nurse, move.nurse2);
            }
            bool noExchangeTabu(ref Move move) {
                Assign a = AssignTable[move.nurse, move.weekday];
                Assign a2 = AssignTable[move.nurse, move.weekday2];

                if (a.IsWorking) {
                    if (a2.IsWorking) {
                        return ((iterCount > shiftTabu[move.nurse, move.weekday, a2.shift, a2.skill])
                            || (iterCount > shiftTabu[move.nurse, move.weekday2, a.shift, a.skill]));
                    } else {
                        return ((iterCount > dayTabu[move.nurse, move.weekday])
                            || (iterCount > shiftTabu[move.nurse, move.weekday2, a.shift, a.skill]));
                    }
                } else {
                    if (a2.IsWorking) {
                        return ((iterCount > shiftTabu[move.nurse, move.weekday, a2.shift, a2.skill])
                            || (iterCount > dayTabu[move.nurse, move.weekday2]));
                    } else {    // no change
                        return true;
                    }
                }
            }
#if !INRC2_BLOCK_SWAP_NO_TABU
            bool noBlockSwapTabu(int noTabuCount, int totalSwap) {
#if INRC2_BLOCK_SWAP_AVERAGE_TABU
                return (2 * noTabuCount > totalSwap);   // over half of swaps are no tabu
#elif INRC2_BLOCK_SWAP_STRONG_TABU
                return (noTabuCount == totalSwap);
#elif INRC2_BLOCK_SWAP_WEAK_TABU
                return (noTabuCount > 0);
#endif
            }
#endif

            bool aspirationCritiera(ObjValue bestMoveDelta, ObjValue bestMoveDelta_Tabu) {
                return ((bestMoveDelta >= DefaultPenalty.MaxObjValue)
                    || ((ObjValue + bestMoveDelta_Tabu < optima.ObjValue)
                    && (bestMoveDelta_Tabu < bestMoveDelta)));
            }

            void updateDayTabu(NurseID nurse, Weekday weekday) {
                dayTabu[nurse, weekday] = iterCount
                    + solver.DayTabuTenureBase + solver.rand.Next(solver.DayTabuTenureAmp);
            }
            void updateShiftTabu(NurseID nurse, Weekday weekday, Assign a) {
                shiftTabu[nurse, weekday, a.shift, a.skill] = iterCount
                    + solver.ShiftTabuTenureBase + solver.rand.Next(solver.ShiftTabuTenureAmp);
            }
            void updateAddTabu(ref Move move) {
                updateDayTabu(move.nurse, move.weekday);
            }
            void updateChangeTabu(ref Move move) {
                updateShiftTabu(move.nurse, move.weekday, AssignTable[move.nurse, move.weekday]);
            }
            void updateRemoveTabu(ref Move move) {
                updateShiftTabu(move.nurse, move.weekday, AssignTable[move.nurse, move.weekday]);
            }
            void updateSwapTabu(Weekday weekday, NurseID nurse, NurseID nurse2) {
                Assign a = AssignTable[nurse, weekday];
                Assign a2 = AssignTable[nurse2, weekday];

                if (a.IsWorking) {
                    if (a2.IsWorking) {
                        updateShiftTabu(nurse, weekday, a);
                        updateShiftTabu(nurse2, weekday, a2);
                    } else {
                        updateShiftTabu(nurse, weekday, a);
                        updateDayTabu(nurse2, weekday);
                    }
                } else {
                    if (a2.IsWorking) {
                        updateDayTabu(nurse, weekday);
                        updateShiftTabu(nurse2, weekday, a2);
                    }
                }
            }
            void updateSwapTabu(ref Move move) {
                updateSwapTabu(move.nurse2, move.nurse2, move.nurse2);
            }
            void updateExchangeTabu(ref Move move) {
                Assign a = AssignTable[move.nurse, move.weekday];
                Assign a2 = AssignTable[move.nurse, move.weekday2];

                if (a.IsWorking) {
                    if (a2.IsWorking) {
                        updateShiftTabu(move.nurse, move.weekday, a);
                        updateShiftTabu(move.nurse, move.weekday2, a2);
                    } else {
                        updateShiftTabu(move.nurse, move.weekday, a);
                        updateDayTabu(move.nurse, move.weekday2);
                    }
                } else {
                    if (a2.IsWorking) {
                        updateDayTabu(move.nurse, move.weekday);
                        updateShiftTabu(move.nurse, move.weekday2, a2);
                    }
                }
            }
            void updateBlockSwapTabu(ref Move move) {
                Weekday weekday = move.weekday;
                Weekday weekday2 = move.weekday2;
                for (; weekday <= weekday2; weekday++) {
                    updateSwapTabu(weekday, move.nurse, move.nurse2);
                }
            }
#endif

            /// <summary>
            /// find day number to be punished for a single block.
            /// (work for shift, day and day-off)
            /// </summary>
            private static ObjValue penaltyDayNum(int len, int end, int min, int max) {
                return (end < Weekdays.Sun) ?
                    Util.distanceToRange(len, min, max) :
                    Util.exceedCount(len, max);
            }
            #endregion Method

            #region Property
            public Output Optima {
                get { return optima; }
                protected set { value.copyTo(optima); }
            }

            /// <summary> short hand for scenario. </summary>
            protected Problem.ScenarioInfo S { get { return problem.Scenario; } }
            /// <summary> short hand for weekdata. </summary>
            protected Problem.WeekdataInfo W { get { return problem.Weekdata; } }
            /// <summary> short hand for history. </summary>
            protected Problem.HistoryInfo H { get { return problem.History; } }
            #endregion Property

            #region Type
            /// <summary> single move in neighborhood search. </summary>
            public struct Move
            {
                public Move(MoveMode mode, ObjValue delta = DefaultPenalty.ForbiddenMove)
                    : this() {
                    this.delta = delta;
                    this.mode = mode;
                }

                public int ModeIndex { get { return (int)mode; } }

                public ObjValue delta;

                public MoveMode mode;

                // weekday2 should always be greater than weekday in block swap.
                public Weekday weekday;
                public Weekday weekday2;

                public NurseID nurse;
                public NurseID nurse2;

                public Assign assign;
            }

            /// <summary> consecutive information for a nurse. </summary>
            /// <example>
            /// this is an demo for switching consecutive working day and day off.
            /// consecutive assignments use the same method.
            /// besides, there is 1 slot for history and update towards left side 
            /// should test if weekday is greater or equal to Weekdays.LastWeek which is 0.
            /// 
            ///    E L O L E E E
            ///   +-+-+-+-+-+-+-+
            ///   |0|1|2|3|4|4|4|  high
            ///   |0|1|2|3|6|6|6|  low
            ///   +-+-+-+-+-+-+-+        +-+-+-+-+-+-+-+
            ///      |       |  E->O     |0|1|2|3|4|5|6|
            ///      | L->O  +---------->|0|1|2|3|4|5|6|
            ///      |                   +-+-+-+-+-+-+-+
            ///      |  +-+-+-+-+-+-+-+
            ///      +->|0|1|1|3|3|3|6|
            ///         |0|2|2|5|5|5|6| 
            ///         +-+-+-+-+-+-+-+
            /// </example>
            private class Consecutive : Util.ICopyable<Consecutive>
            {
                #region Constructor
                public Consecutive() { }
                public Consecutive(Problem p, NurseID nurse) {
                    if (ShiftIDs.isWorking(p.History.lastShifts[nurse])) {
                        dayLow.fill(Weekdays.Mon);
                        dayHigh.fill(Weekdays.Sun);
                        shiftLow.fill(Weekdays.Mon);
                        shiftHigh.fill(Weekdays.Sun);
                        dayHigh[Weekdays.LastWeek] = Weekdays.LastWeek;
                        dayLow[Weekdays.LastWeek] = 1 - p.History.consecutiveDayNums[nurse];
                        shiftHigh[Weekdays.LastWeek] = Weekdays.LastWeek;
                        shiftLow[Weekdays.LastWeek] = 1 - p.History.consecutiveShiftNums[nurse];
                    } else {    // day off
                        dayLow.fill(1 - p.History.consecutiveDayoffNums[nurse]);
                        dayHigh.fill(Weekdays.Sun);
                        shiftLow.fill(1 - p.History.consecutiveDayoffNums[nurse]);
                        shiftHigh.fill(Weekdays.Sun);
                    }
                }
                #endregion Constructor

                #region Method
                public void copyTo(Consecutive consecutive) {
                    dayLow.CopyTo(consecutive.dayLow, 0);
                    dayHigh.CopyTo(consecutive.dayHigh, 0);
                    shiftLow.CopyTo(consecutive.shiftLow, 0);
                    shiftHigh.CopyTo(consecutive.shiftHigh, 0);
                }
                #endregion Method

                #region Property
                /// <summary> if a shift lasts whole week, return true, else false. </summary>
                public bool IsSingleConsecutiveShift {
                    // shiftLow may be LastWeek, so use less or equal
                    get { return (shiftLow[Weekdays.Sun] <= Weekdays.Mon); }
                }
                /// <summary> if a day or day-off lasts whole week, return true, else false. </summary>
                public bool IsSingleConsecutiveDay {
                    // dayLow may be LastWeek, so use less or equal
                    get { return (dayLow[Weekdays.Sun] <= Weekdays.Mon); }
                }
                #endregion Property

                #region Type
                #endregion Type

                #region Constant
                #endregion Constant

                #region Field
                public int[] dayLow = new int[Weekdays.Length];
                public int[] dayHigh = new int[Weekdays.Length];
                public int[] shiftLow = new int[Weekdays.Length];
                public int[] shiftHigh = new int[Weekdays.Length];
                #endregion Field
            }

            /// <summary> information for update delta and assignment. </summary>
            // TUNEUP[1]: struct or class? (attention assignments on name alias)
            class BlockSwapCacheItem
            {
                public ObjValue delta;
                public Weekday weekday;
                public Weekday weekday2;
            }

            public delegate void FindBestMove(ref Move move);
            public delegate ObjValue TryMove(ref Move move);
            public delegate void ApplyMove(ref Move move);
            public delegate void UpdateTabu(ref Move move);
            #endregion Type

            #region Constant
            private readonly TabuSolver solver;
            private readonly Problem problem;

            private readonly FindBestMove[] findBestMove;
            private readonly TryMove[] tryMove;
            private readonly ApplyMove[] applyMove;
            private readonly UpdateTabu[] updateTabu;
            #endregion Constant

            #region Field
            /// <summary> local optima in the trajectory of current search() call. </summary>
            /// <remarks> must set optima to $this before every search(). </remarks>
            Output optima;

            /// <summary> control penalty weight on each constraint. </summary>
            private Penalty penalty;    // TODO[9]: rename to ConstraintWeights?
            /// <summary> control penalty weight on each nurse. </summary>
            private ObjValue[] nurseWeights;

            private IterCount iterCount;

            /// <summary> total assignments for each nurse. </summary>
            private int[] totalAssignNums;
            /// <summary> 
            /// problem.Weekdata.optNurseNums[day,shift,skill] =
            /// (assignedNurseNums[day,shift,skill] + missingNurseNums[day,shift,skill]). 
            /// </summary>
            private int[, ,] assignedNurseNums;
            /// <summary> 
            /// problem.Weekdata.optNurseNums[day,shift,skill] =
            /// (assignedNurseNums[day,shift,skill] + missingNurseNums[day,shift,skill]). 
            /// </summary>
            private int[, ,] missingNurseNums;
            /// <summary> consecutive[nurse] is the consecutive assignments record for nurse. </summary>
            private Consecutive[] consecutives;
            /// <summary> default consecutive for resetAssistData(). </summary>
            private Consecutive[] consecutives_Backup;

            //// <summary>
            ///	BlockSwapCache[nurse,nurse2] stores
            /// delta of best block swap with nurse and nurse2.
            /// </summary>
            /// <remarks> rebuild() and weight adjustment will invalidate all items. </remarks>
            private BlockSwapCacheItem[,] blockSwapCache;
            // (blockSwapDeltaCacheValidFlag[nurse] == false) means 
            // the delta related to nurse can not be reused
            private bool[] isBlockSwapCacheValid;

#if INRC2_USE_TABU
            /// <summary>
            /// fine-grained tabu list for add or remove on each shift
            /// (iterCount <= ShiftTabu[nurse,weekday,shift,skill]) means forbid to be added
            /// </summary>
            private IterCount[, , ,] shiftTabu;
            /// <summary>
            /// coarse-grained tabu list for add or remove on each day
            /// (iterCount <= DayTabu[nurse,weekday]) means forbid to be removed. 
            /// </summary>
            private IterCount[,] dayTabu;
#endif // INRC2_USE_TABU

#if !INRC2_BLOCK_SWAP_CACHED
            private NurseID findBestBlockSwap_StartNurse;
#endif // !INRC2_BLOCK_SWAP_CACHED
            #endregion Field
        }

        public class Configure : SolverConfigure
        {
            #region Constructor
            public Configure(int id, string instanceName, int randSeed, Duration timeout)
                : base(id, instanceName, randSeed, timeout) {
                // TODO[0]: update default configuration to optima.
                this.maxNoImproveFactor = 1;
                this.dayTabuFactors = TabuTenureFactor.DefaultFactor;
                this.shiftTabuFactors = TabuTenureFactor.DefaultFactor;

                this.initPerturbStrength = 0.2;
                this.perturbStrengthDelta = 0.01;
                this.maxPerturbStrength = 0.6;

                this.perturbOriginSelectRatio = 4;
                this.biasTabuSearchOriginSelectRatio = 2;

                this.minTabuBase = 6;
                this.inverseTabuAmpRatio = 4;

                this.inverseTotalBiasRatio = 4;
                this.inversePenaltyBiasRatio = 5;

                this.modeSequence = new MoveMode[] { MoveMode.Add, MoveMode.Change, MoveMode.BlockSwap, MoveMode.Remove };
            }
            #endregion Constructor

            #region Method
            #endregion Method

            #region Property
            #endregion Property

            #region Type

            public struct TabuTenureFactor
            {
                public double tableSize;
                public double nurseNum;
                public double dayNum;
                public double shiftNum;

                public static readonly TabuTenureFactor DefaultFactor
                     = new TabuTenureFactor { tableSize = 0, nurseNum = 0, dayNum = 1, shiftNum = 0 };
            };
            #endregion Type

            #region Constant
            #endregion Constant

            #region Field
            public double maxNoImproveFactor;
            public TabuTenureFactor dayTabuFactors;
            public TabuTenureFactor shiftTabuFactors;

            public double initPerturbStrength;
            public double perturbStrengthDelta;
            public double maxPerturbStrength;

            /// <summary> inverse possibility of starting perturb from optima in last search. </summary> 
            public int perturbOriginSelectRatio;
            /// <summary> inverse possibility of starting bias tabu search from optima in last search. </summary> 
            public int biasTabuSearchOriginSelectRatio;

            /// <summary> minimal tabu tenure base. </summary>
            public int minTabuBase;
            /// <summary> equals to (tabuTenureBase / tabuTenureAmp). </summary>
            public int inverseTabuAmpRatio;

            /// <summary> ratio of biased nurse number in total nurse number. </summary>
            public int inverseTotalBiasRatio;
            /// <summary> ratio of biased nurse selected by penalty of each nurse. </summary>
            public int inversePenaltyBiasRatio;

            public MoveMode[] modeSequence;
            #endregion Field
        }

        private delegate void GenerateInitSolution();
        private delegate void SearchForOptima();
        #endregion Type

        #region Constant
        /// <summary>
        /// fundamental move modes in local search, Length is the number of move types
        /// AR stands for "Add and Remove", "Rand" means select one to search randomly,
        /// "Both" means search both, "Loop" means switch to another when no improvement.
        /// </summary>
        // TUNEUP[4]: make them int to avoid type conversion.
        public enum MoveMode
        {
            // atomic moves are not composed by other moves
            Add, Remove, Change, AtomicMovesLength,
            // basic moves are used in randomWalk()
            Exchange = AtomicMovesLength, Swap, BlockSwap, BasicMovesLength,
            // compound moves which can not be used by a single try() and no apply()
            // for them. apply them by calling apply() of corresponding basic move
            BlockShift = BasicMovesLength, ARRand, ARBoth, Length
        };

        private const int InverseRatioOfRepairTimeInInit = 8;

        private readonly GenerateInitSolution[] generateInitSolution;
        private readonly SearchForOptima[] searchForOptima;
        #endregion Constant

        #region Field
        public IterCount DayTabuTenureBase { get; private set; }
        public IterCount DayTabuTenureAmp { get; private set; }
        public IterCount ShiftTabuTenureBase { get; private set; }
        public IterCount ShiftTabuTenureAmp { get; private set; }

        public IterCount MaxNoImproveForSingleNeighborhood { get; private set; }
        public IterCount MaxNoImproveForAllNeighborhood { get; private set; }
        public IterCount MaxNoImproveForBiasTabuSearch { get; private set; }
        public IterCount MaxNoImproveSwapChainLength { get; private set; }
        public IterCount MaxSwapChainRestartCount { get; private set; }

        private Solution sln;

        private Configure config;

        /// <summary> (nursesHasSameSkill[nurse,nurse2] == true) means they have same skill. </summary>
        private bool[,] nursesHasSameSkill;

        private int[] restMinShiftNum;          // assignments in the planning horizon
        private int[] restMaxShiftNum;          // assignments in the planning horizon
        private int[] restMaxWorkingWeekendNum; // assignments in the planning horizon

        private int totalOptNurseNum;
        #endregion Field
    }
    #endregion solver
}

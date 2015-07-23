#region performance switch
// comment to reduce error messages and log
#define INRC2_LOG

// comment to reduce unnecessary calculation
#define INRC2_DEBUG

// comment to reduce console output
//#define INRC2_PERFORMANCE_TEST
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
#else   // must be no tabu if global setting is no tabu
#define INRC2_BLOCK_SWAP_NO_TABU
#endif

// [fix] grain of block swap
//#define INRC2_BLOCK_SWAP_ORGN
//#define INRC2_BLOCK_SWAP_FAST
//#define INRC2_BLOCK_SWAP_PART
//#define INRC2_BLOCK_SWAP_RAND
#define INRC2_BLOCK_SWAP_CACHED

// comment to use double head version of swap chain 
//#define INRC2_SWAP_CHAIN_DOUBLE_HEAD

// comment to keep searching after ACER improved the worsened nurse
//#define INRC2_SWAP_CHAIN_MAKE_BAD_MOVE
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
    using Duration = Double;
    using TimePoint = Int64;
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

    public static class EnumContractID
    {
        public const ContractID None = Begin - 1;
        public const ContractID Begin = default(ContractID);
    }

    public static class Weekdays
    {
        public const Weekday LastWeek = 0;          // sentinel
        public const Weekday Mon = LastWeek + 1;
        public const Weekday Tue = Mon + 1;
        public const Weekday Wed = Tue + 1;
        public const Weekday Thu = Wed + 1;
        public const Weekday Fri = Thu + 1;
        public const Weekday Sat = Fri + 1;
        public const Weekday Sun = Sat + 1;
        public const Weekday NextWeek = Sun + 1;    // sentinel

        /// <summary> take all sentinel into consideration (recommended). </summary>
        public const Weekday Length = NextWeek + 1;
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
            scenario = new ScenarioInfo();
            names = new NameInfo();

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
            for (SkillID sk = SkillIDs.Begin; sk < scenario.SkillsLength; ++i, ++sk) {
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
            for (ShiftID sh = ShiftIDs.Begin; sh < names.shiftNames.Length; ++i, ++sh) {
                string name = input.scenario.shiftTypes[i].id;
                names.shiftNames[sh] = name;
                names.shiftMap[name] = sh;
                scenario.shifts[sh].minConsecutiveShiftNum =
                    input.scenario.shiftTypes[i].minimumNumberOfConsecutiveAssignments;
                scenario.shifts[sh].maxConsecutiveShiftNum =
                    input.scenario.shiftTypes[i].maximumNumberOfConsecutiveAssignments;
                scenario.shifts[sh].legalNextShifts = Util.CreateArray(length, true);
            }
            for (i = 0; i < input.scenario.forbiddenShiftTypeSuccessions.Length; ++i) {
                Input_INRC2Json.ScenarioInfo.ForbiddenShiftTypeSuccession succession = input.scenario.forbiddenShiftTypeSuccessions[i];
                ShiftID priorShift = names.shiftMap[succession.precedingShiftType];
                bool[] legalNextShift = scenario.shifts[priorShift].legalNextShifts;
                for (int j = 0; j < succession.succeedingShiftTypes.Length; ++j) {
                    ShiftID nextShift = names.shiftMap[succession.succeedingShiftTypes[j]];
                    scenario.shifts[priorShift].legalNextShifts[nextShift] = false;
                }
            }

            length = input.scenario.contracts.Length + EnumContractID.Begin;
            scenario.contracts = new ScenarioInfo.Contract[length];
            names.contractNames = new string[length];
            names.contractMap = new Dictionary<string, ContractID>();
            i = 0;
            for (ContractID c = EnumContractID.Begin; c < names.contractNames.Length; ++i, ++c) {
                string name = input.scenario.contracts[i].id;
                names.contractNames[c] = name;
                names.contractMap[name] = c;
                Input_INRC2Json.ScenarioInfo.Contract inContract = input.scenario.contracts[i];
                ScenarioInfo.Contract contract = scenario.contracts[c];
                contract.minShiftNum = inContract.minimumNumberOfAssignments;
                contract.maxShiftNum = inContract.maximumNumberOfAssignments;
                contract.maxWorkingWeekendNum = inContract.maximumNumberOfWorkingWeekends;
                contract.completeWeekend = Convert.ToBoolean(inContract.completeWeekends);
                contract.minConsecutiveDayNum = inContract.minimumNumberOfConsecutiveWorkingDays;
                contract.maxConsecutiveDayNum = inContract.maximumNumberOfConsecutiveWorkingDays;
                contract.minConsecutiveDayoffNum = inContract.minimumNumberOfConsecutiveDaysOff;
                contract.maxConsecutiveDayoffNum = inContract.maximumNumberOfConsecutiveDaysOff;
            }

            length = input.scenario.nurses.Length + NurseIDs.Begin;
            scenario.nurses = new ScenarioInfo.Nurse[length];
            names.nurseNames = new string[length];
            names.nurseMap = new Dictionary<string, NurseID>();
            i = 0;
            for (NurseID n = NurseIDs.Begin; n < names.nurseNames.Length; ++i, ++n) {
                string name = input.scenario.nurses[i].id;
                names.nurseNames[n] = name;
                names.nurseMap[name] = n;
                Input_INRC2Json.ScenarioInfo.Nurse inNurse = input.scenario.nurses[i];
                ScenarioInfo.Nurse nurse = scenario.nurses[n];
                nurse.contract = names.contractMap[inNurse.contract];
                nurse.skills = new bool[scenario.SkillsLength]; // default value is false which means no such skill
                for (int j = 0; j < inNurse.skills.Length; ++j) {
                    nurse.skills[names.skillMap[inNurse.skills[j]]] = true;
                }
            }
        }

        /// <summary> weekdata must be loaded after scenario. </summary>
        public void loadWeekdata(Input_INRC2Json input) {
            weekdata = new WeekdataInfo();

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
            history = new HistoryInfo();

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
            for (SkillID sk = ShiftIDs.Begin; sk < scenario.SkillsLength; ++sk) {
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
            : this(problem.Scenario.nurses.Length, Weekdays.Length) {
        }

        /// <summary> mainly used for debugging. </summary>
        public Output(int nursesLength, int weekdaysLength, ObjValue objValue,
            SecondaryObjValue secondaryObjValue, TimePoint findTime, string assignStr)
            : this(nursesLength, weekdaysLength, objValue, secondaryObjValue, findTime) {
            int i = 0;
            string[] s = assignStr.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            for (NurseID nurse = NurseIDs.Begin; nurse < nursesLength; ++nurse) {
                for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; ++weekday) {
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
        /// use indexer to access single elements.
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
        public SolverConfigure(int id, string instanceName, int randSeed, Duration timeout) {
            // TODO[0]: update default configuration to optima.
            this.id = id;
            this.instanceName = instanceName;
            this.randSeed = randSeed;
            this.timeout = timeout;
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
        public Duration timeout;
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
            this.endTimeInTicks = (TimePoint)(config.timeout * Durations.Frequency);
            this.endTimeInMilliseconds = (TimePoint)(config.timeout * Durations.MillisecondsInSecond);
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
            for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (ShiftID shift = ShiftIDs.Begin; shift < problem.Scenario.shifts.Length; shift++) {
                    for (SkillID skill = SkillIDs.Begin; skill < problem.Scenario.SkillsLength; skill++) {
                        if (nurseNums[weekday, shift, skill] < problem.Weekdata.minNurseNums[weekday, shift, skill]) {
                            objValue += DefaultPenalty.Understaff_Repair *
                                (problem.Weekdata.minNurseNums[weekday, shift, skill] - nurseNums[weekday, shift, skill]);
                        }
                    }
                }
            }

            // check H3: Shift type successions
            for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (NurseID nurse = 0; nurse < problem.Scenario.nurses.Length; nurse++) {
                    if (!problem.Scenario.shifts[output[nurse, weekday - 1].shift].legalNextShifts[output[nurse, weekday].shift]) {
                        objValue += DefaultPenalty.Succession_Repair;
                    }
                }
            }

            // check H4: Missing required skill
            for (NurseID nurse = 0; nurse < problem.Scenario.nurses.Length; nurse++) {
                for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    if (!problem.Scenario.nurses[nurse].skills[output[nurse, weekday].skill]) {
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
            for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (ShiftID shift = ShiftIDs.Begin; shift < problem.Scenario.shifts.Length; shift++) {
                    for (SkillID skill = SkillIDs.Begin; skill < problem.Scenario.SkillsLength; skill++) {
                        int missingNurse = (problem.Weekdata.optNurseNums[weekday, shift, skill]
                            - nurseNums[weekday, shift, skill]);
                        if (missingNurse > 0) {
                            objValue += DefaultPenalty.InsufficientStaff * missingNurse;
                        }
                    }
                }
            }

            // check S2: Consecutive assignments (15/30)
            // check S3: Consecutive days off (30)
            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                int consecutiveShift = problem.History.consecutiveShiftNums[nurse];
                int consecutiveDay = problem.History.consecutiveDayNums[nurse];
                int consecutiveDayOff = problem.History.consecutiveDayoffNums[nurse];
                bool shiftBegin = (consecutiveShift != 0);
                bool dayBegin = (consecutiveDay != 0);
                bool dayoffBegin = (consecutiveDayOff != 0);

                checkConsecutiveViolation(ref objValue, output, nurse, Weekdays.Mon, problem.History.lastShifts[nurse],
                    ref consecutiveShift, ref consecutiveDay, ref consecutiveDayOff,
                    ref shiftBegin, ref dayBegin, ref dayoffBegin);

                for (int weekday = Weekdays.Tue; weekday <= Weekdays.Sun; weekday++) {
                    checkConsecutiveViolation(ref objValue, output, nurse, weekday, output[nurse, weekday - 1].shift,
                        ref consecutiveShift, ref consecutiveDay, ref consecutiveDayOff,
                        ref shiftBegin, ref dayBegin, ref dayoffBegin);
                }
                // since penalty was calculated when switching assign, the penalty of last 
                // consecutive assignments are not considered. so finish it here.
                ContractID contractID = problem.Scenario.nurses[nurse].contract;
                Problem.ScenarioInfo.Contract contract = problem.Scenario.contracts[contractID];
                if (dayoffBegin && problem.History.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objValue += DefaultPenalty.ConsecutiveDayOff * Weekdays.Num;
                } else if (consecutiveDayOff > contract.maxConsecutiveDayoffNum) {
                    objValue += DefaultPenalty.ConsecutiveDayOff *
                        (consecutiveDayOff - contract.maxConsecutiveDayoffNum);
                } else if (consecutiveDayOff == 0) {    // working day
                    if (shiftBegin && problem.History.consecutiveShiftNums[nurse] > problem.Scenario.shifts[output[nurse, Weekdays.Sun].shift].maxConsecutiveShiftNum) {
                        objValue += DefaultPenalty.ConsecutiveShift * Weekdays.Num;
                    } else if (consecutiveShift > problem.Scenario.shifts[output[nurse, Weekdays.Sun].shift].maxConsecutiveShiftNum) {
                        objValue += DefaultPenalty.ConsecutiveShift *
                            (consecutiveShift - problem.Scenario.shifts[output[nurse, Weekdays.Sun].shift].maxConsecutiveShiftNum);
                    }
                    if (dayBegin && problem.History.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                        objValue += DefaultPenalty.ConsecutiveDay * Weekdays.Num;
                    } else if (consecutiveDay > contract.maxConsecutiveDayNum) {
                        objValue += DefaultPenalty.ConsecutiveDay *
                            (consecutiveDay - contract.maxConsecutiveDayNum);
                    }
                }
            }

            // check S4: Preferences (10)
            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    if (problem.Weekdata.shiftOffs[weekday, output[nurse, weekday].shift, nurse]) {
                        objValue += DefaultPenalty.Preference;
                    }
                }
            }

            // check S5: Complete weekend (30)
            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                if (problem.Scenario.contracts[problem.Scenario.nurses[nurse].contract].completeWeekend
                    && (output[nurse, Weekdays.Sat].IsWorking != output[nurse, Weekdays.Sun].IsWorking)) {
                    objValue += DefaultPenalty.CompleteWeekend;
                }
            }

            // check S6: Total assignments (20)
            // check S7: Total working weekends (30)
            if (problem.History.currentWeek == problem.Scenario.totalWeekNum) {
                for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                    int assignNum = problem.History.totalAssignNums[nurse];
                    for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                        if (output[nurse, weekday].IsWorking) { assignNum++; }
                    }

                    int min = problem.Scenario.contracts[problem.Scenario.nurses[nurse].contract].minShiftNum;
                    int max = problem.Scenario.contracts[problem.Scenario.nurses[nurse].contract].maxShiftNum;
                    objValue += DefaultPenalty.TotalAssign * Util.distanceToRange(assignNum, min, max);

                    int maxWeekend = problem.Scenario.contracts[problem.Scenario.nurses[nurse].contract].maxWorkingWeekendNum;
                    int weekendNum = problem.History.totalWorkingWeekendNums[nurse];
                    if (output[nurse, Weekdays.Sat].IsWorking || output[nurse, Weekdays.Sun].IsWorking) {
                        weekendNum++;
                    }
                    objValue += DefaultPenalty.TotalWorkingWeekend * Util.exceedCount(weekendNum, maxWeekend) / problem.Scenario.totalWeekNum;
                }
            }

            return objValue;
        }

        /// <summary> check objective value on optima. </summary>
        public ObjValue checkObjValue() {
            return checkObjValue(Optima);
        }

        private void checkConsecutiveViolation(ref ObjValue objValue,
            Output output, NurseID nurse, int weekday, ShiftID lastShiftID,
            ref int consecutiveShift, ref int consecutiveDay, ref int consecutiveDayOff,
            ref bool shiftBegin, ref bool dayBegin, ref bool dayoffBegin) {
            ContractID contractID = problem.Scenario.nurses[nurse].contract;
            Problem.ScenarioInfo.Contract contract = problem.Scenario.contracts[contractID];
            ShiftID shift = output[nurse, weekday].shift;
            if (ShiftIDs.isWorking(shift)) {    // working day
                if (consecutiveDay == 0) {  // switch from consecutive day off to working
                    if (dayoffBegin) {
                        if (problem.History.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                            objValue += DefaultPenalty.ConsecutiveDayOff * (weekday - Weekdays.Mon);
                        } else {
                            objValue += DefaultPenalty.ConsecutiveDayOff * Util.distanceToRange(consecutiveDayOff,
                                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum);
                        }
                        dayoffBegin = false;
                    } else {
                        objValue += DefaultPenalty.ConsecutiveDayOff * Util.distanceToRange(consecutiveDayOff,
                            contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum);
                    }
                    consecutiveDayOff = 0;
                    consecutiveShift = 1;
                } else {    // keep working
                    if (shift == lastShiftID) {
                        ++consecutiveShift;
                    } else { // another shift
                        Problem.ScenarioInfo.Shift lastShift = problem.Scenario.shifts[lastShiftID];
                        if (shiftBegin) {
                            if (problem.History.consecutiveShiftNums[nurse] > lastShift.maxConsecutiveShiftNum) {
                                objValue += DefaultPenalty.ConsecutiveShift * (weekday - Weekdays.Mon);
                            } else {
                                objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                                    lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum);
                            }
                            shiftBegin = false;
                        } else {
                            objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                                lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum);
                        }
                        consecutiveShift = 1;
                    }
                }
                ++consecutiveDay;
            } else {    // day off
                if (consecutiveDayOff == 0) {   // switch from consecutive working to day off
                    Problem.ScenarioInfo.Shift lastShift = problem.Scenario.shifts[lastShiftID];
                    if (shiftBegin) {
                        if (problem.History.consecutiveShiftNums[nurse] > lastShift.maxConsecutiveShiftNum) {
                            objValue += DefaultPenalty.ConsecutiveShift * (weekday - Weekdays.Mon);
                        } else {
                            objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                                lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum);
                        }
                        shiftBegin = false;
                    } else {
                        objValue += DefaultPenalty.ConsecutiveShift * Util.distanceToRange(consecutiveShift,
                            lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum);
                    }
                    if (dayBegin) {
                        if (problem.History.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                            objValue += DefaultPenalty.ConsecutiveDay * (weekday - Weekdays.Mon);
                        } else {
                            objValue += DefaultPenalty.ConsecutiveDay * Util.distanceToRange(consecutiveDay,
                                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum);
                        }
                        dayBegin = false;
                    } else {
                        objValue += DefaultPenalty.ConsecutiveDay * Util.distanceToRange(consecutiveDay,
                            contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum);
                    }
                    consecutiveShift = 0;
                    consecutiveDay = 0;
                }
                ++consecutiveDayOff;
            }
        }

        /// <summary> generate nurse numbers in $nurseNums[day,shift,skill]. </summary>
        private void countNurseNums(Output output) {
            if (nurseNums == null) {
                nurseNums = new int[Weekdays.Length,
                    problem.Scenario.shifts.Length, problem.Scenario.SkillsLength];
            }

            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    Assign a = output[nurse, weekday];
                    nurseNums[weekday, a.shift, a.skill]++;
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
                .Append((Optima.ObjValue + problem.History.accObjValue) / DefaultPenalty.Amp).Append(",");

            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                    Assign assign = Optima[nurse, weekday];
                    log.Append(assign.shift).Append(" ").Append(assign.skill).Append(" ");
                }
            }

            File.AppendAllText(logFileName, log.ToString());
        }

        /// <summary> log to console with time and runID. </summary>
        public void errorLog(string msg) {
#if INRC2_LOG
            Console.Error.Write(DateTime.Now.ToString(Durations.TimeFormat_Readable));
            Console.Error.Write("," + Config.id + ",");
            Console.Error.WriteLine(msg);
#endif
        }

        /// <summary> generate history for next week from optima. </summary>
        public abstract Problem.HistoryInfo generateHistory();

        /// <summary> return true if global optima or population is updated. </summary>
        protected abstract bool updateOptima(Output localOptima);
        #endregion Method

        #region Property
        public Output Optima {
            get { return optima; }
            protected set { value.copyTo(optima); }
        }

        public bool IsTimeout { get { return (clock.ElapsedTicks > endTimeInTicks); } }
        public Duration TimeLeft { get { return (endTimeInMilliseconds - clock.ElapsedMilliseconds); } }

        protected abstract TConfig Config { get; set; }
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
            void reset() {
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

            void recoverLastMode() {
                pm = modeStack.Pop();
            }

            void setSwapMode() {
                modeStack.Push(pm);

                pm.understaff = 0;          // due to no extra assignments
                pm.insufficientStaff = 0;   // due to no extra assignments
            }

            void setBlockSwapMode() {
                modeStack.Push(pm);

                pm.understaff = 0;  // due to no extra assignments
                pm.succession = 0;  // due to it is checked manually
                pm.missSkill = 0;   // due to it is checked manually
                pm.insufficientStaff = 0;   // due to no extra assignments
            }

            void setExchangeMode() {
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
            void setRepairMode(ObjValue WeightOnUnderStaff = DefaultPenalty.Understaff_Repair,
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
        public const Duration SaveSolutionTime = 500;
        #endregion Constant

        #region Field
        public readonly Problem problem;

        protected readonly Random rand;
        protected readonly Stopwatch clock;
        protected IterCount iterationCount;
        protected IterCount generationCount;

        private Output optima;

        /// <summary> hold the array to avoid reallocation in countNurseNums(). </summary>
        private int[, ,] nurseNums;

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
            // TODO[9]: make them static member? 
            //          (turn init and search into static method with a parameter of a TabuSolver)
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

        public override Problem.HistoryInfo generateHistory() {
            Problem.HistoryInfo newHistory = new Problem.HistoryInfo();

            // UNDONE[0]: generate history from assign table.

            return newHistory;
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
#else
                bool isSelected = ((rand.Next(2)) == 0);
#endif
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
            restMinShiftNum = new int[problem.Scenario.nurses.Length];
            restMaxShiftNum = new int[problem.Scenario.nurses.Length];
            restMaxWorkingWeekendNum = new int[problem.Scenario.nurses.Length];

            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; ++nurse) {
                Problem.ScenarioInfo.Contract c = problem.Scenario.contracts[problem.Scenario.nurses[nurse].contract];
#if INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
                int weekToStartCountMin = problem.Scenario.totalWeekNum * c.minShiftNum;
                bool ignoreMinShift = ((problem.History.currentWeek * c.maxShiftNum) < weekToStartCountMin);
                restMinShiftNum[nurse] = (ignoreMinShift) ? 0 : (c.minShiftNum - problem.History.totalAssignNums[nurse]);
#else
                restMinShiftNum[nurse] = c.minShiftNum - problem.History.totalAssignNums[nurse];
#endif
                restMaxShiftNum[nurse] = c.maxShiftNum - problem.History.totalAssignNums[nurse];
                restMaxWorkingWeekendNum[nurse] = c.maxWorkingWeekendNum - problem.History.totalWorkingWeekendNums[nurse];
            }
        }

        /// <summary> initialize assist data about nurse-skill relation. </summary>
        private void discoverNurseSkillRelation() {
            // UNDONE[1]: discoverNurseSkillRelation
            nursesHasSameSkill = new bool[problem.Scenario.nurses.Length, problem.Scenario.nurses.Length];

            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                for (NurseID nurse2 = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse2++) {
                    nursesHasSameSkill[nurse, nurse2] = problem.haveSameSkill(nurse, nurse2);
                }
            }
        }

        private void countTotalOptNurseNum() {
            totalOptNurseNum = 0;
            for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                for (ShiftID shift = ShiftIDs.Begin; shift < problem.Scenario.shifts.Length; shift++) {
                    for (SkillID skill = SkillIDs.Begin; skill < problem.Scenario.SkillsLength; skill++) {
                        totalOptNurseNum += problem.Weekdata.optNurseNums[weekday, shift, skill];
                    }
                }
            }
        }

        private void setTabuTenure() {
            // plus 1 to make sure it will not be 0
            // table size
            dayTabuTenureBase = (IterCount)(1 + config.dayTabuFactors.tableSize *
                problem.Scenario.NurseNum * Weekdays.Num);
            shiftTabuTenureBase = (IterCount)(1 + config.shiftTabuFactors.tableSize *
                problem.Scenario.NurseNum * Weekdays.Num * problem.Scenario.ShiftNum * problem.Scenario.SkillNum);
            // nurse number
            dayTabuTenureBase *= (IterCount)(1 + config.dayTabuFactors.nurseNum * problem.Scenario.NurseNum);
            shiftTabuTenureBase *= (IterCount)(1 + config.shiftTabuFactors.nurseNum * problem.Scenario.NurseNum);
            // day number
            dayTabuTenureBase *= (IterCount)(1 + config.dayTabuFactors.dayNum * Weekdays.Num);
            shiftTabuTenureBase *= (IterCount)(1 + config.shiftTabuFactors.dayNum * Weekdays.Num);
            // shift number
            dayTabuTenureBase *= (IterCount)(1 + config.dayTabuFactors.shiftNum *
                problem.Scenario.ShiftNum * problem.Scenario.SkillNum);
            shiftTabuTenureBase *= (IterCount)(1 + config.shiftTabuFactors.shiftNum *
                problem.Scenario.ShiftNum * problem.Scenario.SkillNum);

            if (dayTabuTenureBase < config.minTabuBase) { dayTabuTenureBase = config.minTabuBase; }
            if (shiftTabuTenureBase < config.minTabuBase) { shiftTabuTenureBase = config.minTabuBase; }
            dayTabuTenureAmp = 1 + dayTabuTenureBase / config.inverseTabuAmpRatio;
            shiftTabuTenureAmp = 1 + shiftTabuTenureBase / config.inverseTabuAmpRatio;
        }

        private void setMaxNoImprove() {
            MaxNoImproveForSingleNeighborhood = (IterCount)(
                config.maxNoImproveFactor * problem.Scenario.NurseNum * Weekdays.Num);
            MaxNoImproveForAllNeighborhood = (IterCount)(
                config.maxNoImproveFactor * problem.Scenario.NurseNum * Weekdays.Num *
                Math.Sqrt(problem.Scenario.ShiftNum * problem.Scenario.SkillNum));

            MaxNoImproveForBiasTabuSearch = MaxNoImproveForSingleNeighborhood / config.inverseTotalBiasRatio;

            MaxNoImproveSwapChainLength = MaxNoImproveForSingleNeighborhood;
            MaxSwapChainRestartCount = (IterCount)(Math.Sqrt(problem.Scenario.NurseNum));
        }

        private void randomInit() {

        }

        private void greedyInit() {

        }

        private void exactInit() {

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
                this.solver = solver;
                this.problem = solver.problem;

                this.iterCount = 1;

                // init sentinels
                for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                    AssignTable[nurse, Weekdays.LastWeek].shift = problem.History.lastShifts[nurse];
                    AssignTable[nurse, Weekdays.NextWeek].shift = ShiftIDs.None;
                }
            }
            #endregion Constructor

            #region Method
            public void generateInitSolution_Random() {
                resetAssign();
                resetAssistData();

                for (int i = 0; i < solver.totalOptNurseNum; i++) {
                    NurseID nurse = solver.rand.Next(NurseIDs.Begin, problem.Scenario.nurses.Length);
                    Weekday weekday = solver.rand.Next(Weekdays.Mon, Weekdays.NextWeek);
                    Assign assign = new Assign(
                        solver.rand.Next(ShiftIDs.Begin, problem.Scenario.shifts.Length),
                        solver.rand.Next(SkillIDs.Begin, problem.Scenario.SkillsLength));

                    if ((!AssignTable[nurse, weekday].IsWorking)
                        && problem.Scenario.nurses[nurse].skills[assign.skill]) {
                        addAssign(weekday, nurse, assign);
                    }
                }

                // if it does not work out within total timeout, the solution is meaningless
                if (Util.Worker.WorkUntilTimeout(repair, (int)solver.TimeLeft)) {
                    evaluateObjValue(); // TUNEUP[0]: objective might have been evaluated in repair()!
                } else {
                    ObjValue = DefaultPenalty.ForbiddenMove;
                }
            }
            public void generateInitSolution_Greedy() {

            }
            public void generateInitSolution_BranchAndCut() {

            }

            /// <summary>
            /// set assignments to output.AssignTable with about $differentSlotNum
            /// of assignment is different and rebuild assist data.
            /// (output must be build from same problem)
            /// </summary>
            /// <remarks> objValue will be recalculated. </remarks>
            public void rebuild(Output output, int differentSlotNum) {
                int totalSlotNum = problem.Scenario.NurseNum * Weekdays.Num;

                if (differentSlotNum < totalSlotNum) {
                    Output assignments = ((this == output) ? new Output(output) : output);

                    resetAssign();
                    resetAssistData();

                    for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                        for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                            if (assignments[nurse, weekday].IsWorking) {
                                if (solver.rand.Next(totalSlotNum) > differentSlotNum) {
                                    addAssign(weekday, nurse, assignments[nurse, weekday]);
                                }
                            }
                        }
                    }
                }

                // if it does not work out within total timeout, the solution is meaningless
                if (Util.Worker.WorkUntilTimeout(repair, (int)solver.TimeLeft)) {
                    evaluateObjValue(); // TUNEUP[0]: objective might have been evaluated in repair()!
                } else {
                    ObjValue = DefaultPenalty.ForbiddenMove;
                }
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

                for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
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

            /// <summary> set data structure to default value. </summary>
            /// <remarks> must be called before building up a solution. </remarks> 
            private void resetAssign() {
                for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; nurse++) {
                    for (Weekday weekday = Weekdays.Mon; weekday <= Weekdays.Sun; weekday++) {
                        AssignTable[nurse, weekday].shift = ShiftIDs.None;
                    }
                }
            }
            /// <summary> set data structure to default value. </summary>
            /// <remarks> must be called before building up a solution. </remarks> 
            private void resetAssistData() {

            }

            private bool fillAssign(Weekday weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum) {
                if (nurse >= problem.Scenario.nurses.Length) {
                    if (nurseNum < problem.Weekdata.minNurseNums[weekday, shift, skill]) {
                        return false;
                    } else {
                        return fillAssign(weekday, shift, skill + 1, NurseIDs.Begin, 0);
                    }
                } else if (skill >= problem.Scenario.SkillsLength) {
                    return fillAssign(weekday, shift + 1, SkillIDs.Begin, NurseIDs.Begin, 0);
                } else if (shift >= problem.Scenario.shifts.Length) {
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
                    if (problem.Scenario.nurses[nurse].skills[skill]
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

            /// <summary> 
            /// evaluate objective by assist data structure.
            /// must be called after Penalty change or direct access to AssignTable.
            /// </summary>
            private void evaluateObjValue(bool considerSpanningConstraint = true) {

            }
            private ObjValue evaluateInsufficientStaff() { return 0; }
            private ObjValue evaluateConsecutiveShift(NurseID nurse) { return 0; }
            private ObjValue evaluateConsecutiveDay(NurseID nurse) { return 0; }
            private ObjValue evaluateConsecutiveDayOff(NurseID nurse) { return 0; }
            private ObjValue evaluatePreference(NurseID nurse) { return 0; }
            private ObjValue evaluateCompleteWeekend(NurseID nurse) { return 0; }
            private ObjValue evaluateTotalAssign(NurseID nurse) { return 0; }
            private ObjValue evaluateTotalWorkingWeekend(NurseID nurse) { return 0; }

            /// <summary> make infeasible solution feasible. </summary>
            private void repair() {

            }

            // UNDONE[0]: parameter sequence has been changed!
            private bool isValidSuccession(NurseID nurse, Weekday weekday, ShiftID shift) {
                return problem.Scenario.shifts[AssignTable[nurse, weekday - 1].shift].legalNextShifts[shift];
            }
            // UNDONE[0]: parameter sequence has been changed!
            private bool isValidPrior(NurseID nurse, Weekday weekday, ShiftID shift) {
                return problem.Scenario.shifts[shift].legalNextShifts[AssignTable[nurse, weekday + 1].shift];
            }

            // apply assigning a Assign to nurse without Assign in weekday
            private void addAssign(Weekday weekday, NurseID nurse, Assign assign) { }
            private void addAssign(Move move) { }
            // apply assigning another Assign or skill to nurse already assigned in weekday
            private void changeAssign(Weekday weekday, NurseID nurse, Assign assign) { }
            private void changeAssign(Move move) { }
            // apply removing a Assign to nurse in weekday
            private void removeAssign(Weekday weekday, NurseID nurse) { }
            private void removeAssign(Move move) { }
            // apply swapping Assign of two nurses in the same day
            private void swapNurse(Weekday weekday, NurseID nurse, NurseID nurse2) { }
            private void swapNurse(Move move) { }
            // apply swapping Assign of two nurses in consecutive days within [weekday, weekday2]
            private void swapBlock(Weekday weekday, Weekday weekday2, NurseID nurse, NurseID nurse2) { }
            private void swapBlock(Move move) { }
            // apply exchanging Assign of a nurse on two days
            private void exchangeDay(Weekday weekday, NurseID nurse, Weekday weekday2) { }
            private void exchangeDay(Move move) { }

            private void updateConsecutive(Weekday weekday, NurseID nurse, ShiftID shift) { }
            // the assignment is on the right side of a consecutive block
            private void assignHigh(Weekday weekday, Weekday[] high, Weekday[] low, bool affectRight) { }
            // the assignment is on the left side of a consecutive block
            private void assignLow(Weekday weekday, Weekday[] high, Weekday[] low, bool affectLeft) { }
            // the assignment is in the middle of a consecutive block
            private void assignMiddle(Weekday weekday, Weekday[] high, Weekday[] low) { }
            // the assignment is on a consecutive block with single slot
            private void assignSingle(Weekday weekday, Weekday[] high, Weekday[] low, bool affectRight, bool affectLeft) { }
            #endregion Method

            #region Property
            #endregion Property

            #region Type
            /// <summary> single move in neighborhood search. </summary>
            protected class Move
            {
                Move(ObjValue delta, int weekday, int weekday2, NurseID nurse, NurseID nurse2) {
                    this.delta = delta;
                    this.weekday = weekday;
                    this.weekday2 = weekday2;
                    this.nurse = nurse;
                    this.nurse2 = nurse2;
                }

                ObjValue delta = DefaultPenalty.ForbiddenMove;
                MoveMode mode;
                int weekday;    // tryBlockSwap() will modify it
                // weekday2 should always be greater than weekday in block swap
                int weekday2;   // tryBlockSwap() will modify it
                NurseID nurse;
                NurseID nurse2;
                Assign assign;
            }
            #endregion Type

            #region Constant
            private readonly TabuSolver solver;
            private readonly Problem problem;
            #endregion Constant

            #region Field
            private IterCount iterCount;
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
        public enum MoveMode
        {
            // atomic moves are not composed by other moves
            Add, Remove, Change, AtomicMovesLength,
            // basic moves are used in randomWalk()
            Exchange = AtomicMovesLength, Swap, BlockSwap, BasicMovesLength,
            // compound moves which can not be used by a single tryXXX() and no apply()
            // for them. apply them by calling apply() of corresponding basic move
            BlockShift = BasicMovesLength, ARLoop, ARRand, ARBoth, Length
        };

        private readonly GenerateInitSolution[] generateInitSolution;
        private readonly SearchForOptima[] searchForOptima;
        #endregion Constant

        #region Field
        private Solution sln;

        private Configure config;

        private IterCount dayTabuTenureBase;
        private IterCount dayTabuTenureAmp;
        private IterCount shiftTabuTenureBase;
        private IterCount shiftTabuTenureAmp;

        private IterCount MaxNoImproveForSingleNeighborhood;
        private IterCount MaxNoImproveForAllNeighborhood;
        private IterCount MaxNoImproveForBiasTabuSearch;
        private IterCount MaxNoImproveSwapChainLength;
        private IterCount MaxSwapChainRestartCount;

        private bool[,] nursesHasSameSkill;

        private int[] restMinShiftNum;          // assignments in the planning horizon
        private int[] restMaxShiftNum;          // assignments in the planning horizon
        private int[] restMaxWorkingWeekendNum; // assignments in the planning horizon

        private int totalOptNurseNum;
        #endregion Field
    }
    #endregion solver
}


// TODO[5]: record distance to optima after every move, analyze if it is circling around.

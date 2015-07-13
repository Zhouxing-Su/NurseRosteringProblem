using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;


namespace NurseRostering
{
    #region type alias
    using ContractID = Int32;
    using Duration = Int64;
    using IterCount = Int32;
    using NurseID = Int32;
    using ObjValue = Int32;
    using SecondaryObjValue = Double;
    using ShiftID = Int32;
    using SkillID = Int32;
    using Weekday = Int32;
    #endregion

    #region constants
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
        public const Weekday HIS = 0;               // sentinel
        public const Weekday Mon = HIS + 1;
        public const Weekday Tue = Mon + 1;
        public const Weekday Wed = Tue + 1;
        public const Weekday Thu = Wed + 1;
        public const Weekday Fri = Thu + 1;
        public const Weekday Sat = Fri + 1;
        public const Weekday Sun = Sat + 1;
        public const Weekday NEXT_WEEK = Sun + 1;   // sentinel

        /// <summary> not considering NextWeek sentinel. </summary>
        public const Weekday SIZE_MIN = Sun + 1;
        /// <summary> take all sentinel into consideration (recommended). </summary>
        public const Weekday SIZE_FULL = NEXT_WEEK + 1;
        /// <summary> number of days in a week. </summary>
        public const Weekday NUM = Sun - Mon + 1;

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
        public const ObjValue MAX_OBJ_VALUE = (1 << 24);
        /// <summary> tryMove should return FORBIDDEN_MOVE in case it is in a complex move. </summary>
        public const ObjValue FORBIDDEN_MOVE = (MAX_OBJ_VALUE * 2);
        /// <summary> amplifier for improving accuracy. </summary>
        public const ObjValue AMP = 2 * 2 * 2 * 3 * 7;

        #region hard constraints
        public const ObjValue SingleAssign = FORBIDDEN_MOVE;
        public const ObjValue Understaff = FORBIDDEN_MOVE;
        public const ObjValue Succession = FORBIDDEN_MOVE;
        public const ObjValue MissSkill = FORBIDDEN_MOVE;

        public const ObjValue Understaff_Repair = (AMP * 80);
        public const ObjValue Succession_Repair = (AMP * 120);
        #endregion

        #region soft constraints
        public const ObjValue InsufficientStaff = (AMP * 30);
        public const ObjValue ConsecutiveShift = (AMP * 15);
        public const ObjValue ConsecutiveDay = (AMP * 30);
        public const ObjValue ConsecutiveDayOff = (AMP * 30);
        public const ObjValue Preference = (AMP * 10);
        public const ObjValue CompleteWeekend = (AMP * 30);
        public const ObjValue TotalAssign = (AMP * 20);
        public const ObjValue TotalWorkingWeekend = (AMP * 30);
        #endregion
    }
    #endregion

    #region raw input
    /// <summary> original data from instance files. </summary>
    [DataContract]
    public class Input_INRC2Json
    {
        #region Constructor
        #endregion

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
        #endregion
        #endregion

        #region Property
        #endregion

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
        #endregion

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
        #endregion

        #region Field
        [DataMember]
        public ScenarioInfo scenario;
        [DataMember]
        public WeekdataInfo weekdata;
        [DataMember]
        public HistoryInfo history;
        #endregion
    }
    #endregion

    #region solver
    public abstract class Solver
    {
        #region Constructor
        public Solver(Input input) {
            problem = input;
            optima = new Output(input);
            rand = new Random();
        }
        #endregion

        #region Method
        public abstract void init();
        public abstract void solve();
        public void check() {

        }
        public void record() {

        }

        protected abstract bool updateOptima(Output localOptima);
        #endregion

        #region Property
        public Output Optima {
            get {
                return optima;
            }
            protected set {
                value.copyTo(optima);
            }
        }
        #endregion

        #region Type

        #region input and output
        /// <summary> formatted structural input data. </summary>
        [DataContract]
        public class Input
        {
            #region Constructor
            #endregion

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
                weekdata.shiftOffs = new bool[scenario.nurses.Length, Weekdays.SIZE_MIN, scenario.shifts.Length];
                foreach (Input_INRC2Json.WeekdataInfo.ShiftOffRequest request in input.weekdata.shiftOffRequests) {
                    NurseID nurse = names.nurseMap[request.nurse];
                    Weekday weekday = Weekdays.Map[request.day];
                    ShiftID shift = names.shiftMap[request.shiftType];
                    weekdata.shiftOffs[nurse, weekday, shift] = true;
                }

                weekdata.minNurseNums = new int[Weekdays.SIZE_MIN, scenario.shifts.Length, scenario.SkillsLength];
                weekdata.optNurseNums = new int[Weekdays.SIZE_MIN, scenario.shifts.Length, scenario.SkillsLength];
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
                history.lastShfts = new ShiftID[scenario.nurses.Length];
                history.consecutiveShiftNums = new int[scenario.nurses.Length];
                history.consecutiveDayNums = new int[scenario.nurses.Length];
                history.consecutiveDayoffNums = new int[scenario.nurses.Length];
                foreach (Input_INRC2Json.HistoryInfo.NurseHistoryInfo h in input.history.nurseHistory) {
                    NurseID nurse = names.nurseMap[h.nurse];
                    history.totalAssignNums[nurse] = h.numberOfAssignments;
                    history.totalWorkingWeekendNums[nurse] = h.numberOfWorkingWeekends;
                    history.lastShfts[nurse] = names.shiftMap[h.lastAssignedShiftType];
                    history.consecutiveShiftNums[nurse] = h.numberOfConsecutiveAssignments;
                    history.consecutiveDayNums[nurse] = h.numberOfConsecutiveWorkingDays;
                    history.consecutiveDayoffNums[nurse] = h.numberOfConsecutiveDaysOff;
                }
            }

            public void readCustomFile() {

            }

            public void writeCustomFile() {

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

            #endregion

            #region Property
            public ScenarioInfo Scenario { get { return scenario; } }
            public WeekdataInfo Weekdata { get { return weekdata; } }
            public HistoryInfo History { get { return history; } }
            public NameInfo Names { get { return names; } }
            #endregion

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
                /// <summary> (shiftOffs[nurse][day][shift] == true) means shiftOff required. </summary>
                [DataMember]
                public bool[, ,] shiftOffs;

                /// <summary> minNurseNums[day][shift][skill] is a number of nurse. </summary>
                [DataMember]
                public int[, ,] minNurseNums;

                /// <summary> optNurseNums[day][shift][skill] is a number of nurse. </summary>
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
                public ShiftID[] lastShfts;
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
            #endregion

            #region Constant
            #endregion

            #region Field
            [DataMember]
            private ScenarioInfo scenario;
            [DataMember]
            private WeekdataInfo weekdata;
            [DataMember]
            private HistoryInfo history;
            [DataMember]
            private NameInfo names;
            #endregion
        }

        /// <summary> Output[nurse][day] is an Assign. </summary>    
        [DataContract]
        public class Output : Util.ICopyable<Output>
        {
            #region Constructor
            /// <summary>
            /// weekdayNum should be (ActualWeekdayNumber + 2) to let it 
            /// allocate additional days for history and next week sentinel.
            /// </summary>
            public Output(int nurseNum, int weekdayNum, ObjValue objValue = DefaultPenalty.MAX_OBJ_VALUE,
                SecondaryObjValue secondaryObjValue = 0, Duration findTime = 0) {
                assignTable = new Assign[nurseNum, weekdayNum];
                ObjValue = objValue;
                SecondaryObjValue = secondaryObjValue;
                FindTime = findTime;
            }

            public Output(Input input)
                : this(input.Scenario.nurses.Length, Weekdays.SIZE_FULL) {
            }

            public Output(int nurseNum, int weekdayNum, ObjValue objValue,
                SecondaryObjValue secondaryObjValue, Duration findTime, string assignStr)
                : this(nurseNum, weekdayNum, objValue, secondaryObjValue, findTime) {
                int i = 0;
                string[] s = assignStr.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
                for (NurseID nurse = NurseIDs.Begin; nurse < nurseNum; ++nurse) {
                    for (int weekday = Weekdays.Mon; weekday <= Weekdays.Sun; ++weekday) {
                        assignTable[nurse, weekday] =
                            new Assign(ShiftID.Parse(s[i]), SkillID.Parse(s[i + 1]));
                        i += 2;
                    }
                }
            }
            #endregion

            #region Method
            public void copyTo(Output output) {
                output.AssignTable = AssignTable;
                output.ObjValue = ObjValue;
                output.SecondaryObjValue = SecondaryObjValue;
                output.FindTime = FindTime;
            }
            #endregion

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

            public Duration FindTime {
                get { return findTime; }
                protected set { findTime = value; }
            }
            #endregion

            #region Type
            [DataContract]
            public struct Assign
            {
                #region Constructor
                public Assign(ShiftID sh, SkillID sk) {
                    shift = sh;
                    skill = sk;
                }
                #endregion

                #region Method

                #region equality judgment
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
                #endregion
                #endregion

                #region Property
                public bool IsWorking { get { return ShiftIDs.isWorking(shift); } }

                public ShiftID Shift {
                    get { return shift; }
                    set { shift = value; }
                }

                public SkillID Skill {
                    get { return skill; }
                    set { skill = value; }
                }
                #endregion

                #region Type
                #endregion

                #region Constant
                #endregion

                #region Field
                [DataMember]
                private ShiftID shift;
                [DataMember]
                private SkillID skill;
                #endregion
            }
            #endregion

            #region Constant
            #endregion

            #region Field
            private Assign[,] assignTable;
            private ObjValue objValue;
            private SecondaryObjValue secondaryObjValue;
            private Duration findTime;
            #endregion
        }

        [DataContract]
        public class Solution : Output
        {
            #region Constructor
            public Solution(Solver solver)
                : base(solver.problem) {
            }
            #endregion

            #region Method
            #endregion

            #region Property
            #endregion

            #region Type
            #endregion

            #region Constant
            #endregion

            #region Field
            #endregion
        }
        #endregion
        protected class PenaltyMode
        {
            #region hard constraint
            public ObjValue singleAssign;
            public ObjValue understaff;
            public ObjValue succession;
            public ObjValue missSkill;
            #endregion

            #region soft constraint
            public ObjValue insufficientStaff;
            public ObjValue consecutiveShift;
            public ObjValue consecutiveDay;
            public ObjValue consecutiveDayOff;
            public ObjValue preference;
            public ObjValue completeWeekend;
            public ObjValue totalAssign;
            public ObjValue totalWorkingWeekend;
            #endregion
        };

        protected class Penalty
        {
            #region Constructor
            public Penalty() {
                modeStack = new Stack<PenaltyMode>();
                reset();
            }
            #endregion

            #region Method
            // reset to default penalty mode and clear mode stack
            void reset() {
                modeStack.Clear();

                pm.singleAssign = DefaultPenalty.FORBIDDEN_MOVE;
                pm.understaff = DefaultPenalty.FORBIDDEN_MOVE;
                pm.succession = DefaultPenalty.FORBIDDEN_MOVE;
                pm.missSkill = DefaultPenalty.FORBIDDEN_MOVE;

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
            /// allow hard constraints UnderStaff and Succession being violated, <para />
            /// but with much greater penalty than soft constraints. <para />
            /// set softConstraintDecay to MAX_OBJ_VALUE to make them does not count.
            /// </summary>
            void setRepairMode(ObjValue WeightOnUnderStaff = DefaultPenalty.Understaff_Repair,
                ObjValue WeightOnSuccesion = DefaultPenalty.Succession_Repair,
                ObjValue softConstraintDecay = DefaultPenalty.MAX_OBJ_VALUE) {
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
            #endregion

            #region Property
            #region hard constraint
            public ObjValue UnderStaff { get { return pm.understaff; } }
            public ObjValue SingleAssign { get { return pm.singleAssign; } }
            public ObjValue Succession { get { return pm.succession; } }
            public ObjValue MissSkill { get { return pm.missSkill; } }
            #endregion
            #region soft constraint
            public ObjValue InsufficientStaff { get { return pm.insufficientStaff; } }
            public ObjValue ConsecutiveShift { get { return pm.consecutiveShift; } }
            public ObjValue ConsecutiveDay { get { return pm.consecutiveDay; } }
            public ObjValue ConsecutiveDayOff { get { return pm.consecutiveDayOff; } }
            public ObjValue Preference { get { return pm.preference; } }
            public ObjValue CompleteWeekend { get { return pm.completeWeekend; } }
            public ObjValue TotalAssign { get { return pm.totalAssign; } }
            public ObjValue TotalWorkingWeekend { get { return pm.totalWorkingWeekend; } }
            #endregion
            #endregion

            #region Type
            #endregion

            #region Constant
            #endregion

            #region Field
            private PenaltyMode pm;
            private Stack<PenaltyMode> modeStack;
            #endregion
        };

        protected class Config
        {
            #region Constructor
            #endregion

            #region Method
            #endregion

            #region Property
            public int RandSeed { get; private set; }
            public Duration Timeout { get; private set; }
            public IterCount MaxIterCount { get; private set; }
            #endregion

            #region Type
            #endregion

            #region Constant
            #endregion

            #region Field
            #endregion
        }
        #endregion

        #region Constant
        #endregion

        #region Field
        [DataMember]
        public readonly Input problem;
        protected readonly Random rand;

        private Output optima;
        #endregion
    }

    public class TabuSolver : Solver
    {
        #region Constructor
        public TabuSolver(Input input)
            : base(input) {

        }
        #endregion

        #region Method
        public override void init() {
            calculateRestWorkload();
        }

        public override void solve() {

        }

        protected override bool updateOptima(Output localOptima) {
            if (localOptima.ObjValue < Optima.ObjValue) {
                Optima = localOptima;
                return true;
            } else if (localOptima.ObjValue == Optima.ObjValue) {
#if INRC2_SECONDARY_OBJ_VALUE
                bool  isSelected = (localOptima.SecondaryObjValue < Optima.SecondaryObjValue);
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

        /// <summary>
        /// max increase with same delta each week. <para />
        /// do not count min shift number in early weeks (with macro on).
        /// </summary>
        private void calculateRestWorkload() {
            restMinShiftNum = new int[problem.Scenario.nurses.Length];
            restMaxShiftNum = new int[problem.Scenario.nurses.Length];
            restMaxWorkingWeekendNum = new int[problem.Scenario.nurses.Length];

            for (NurseID nurse = NurseIDs.Begin; nurse < problem.Scenario.nurses.Length; ++nurse) {
                Input.ScenarioInfo.Contract c = problem.Scenario.contracts[problem.Scenario.nurses[nurse].contract];
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
        #endregion

        #region Property
        #endregion

        #region Type
        #endregion

        #region Constant
        #endregion

        #region Field

        [DataMember]
        private int[] restMinShiftNum;          // assignments in the planning horizon
        [DataMember]
        private int[] restMaxShiftNum;          // assignments in the planning horizon
        [DataMember]
        private int[] restMaxWorkingWeekendNum; // assignments in the planning horizon
        #endregion
    }
    #endregion
}

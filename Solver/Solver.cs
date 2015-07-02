using System;
using System.Collections.Generic;
using System.IO;


namespace NurseRostering
{
    #region type alias
    using ObjValue = Int32;
    using IterCount = Int32;
    using Duration = Int64;
    using Weekday = Int32;
    using ContractID = Int32;
    using NurseID = Int32;
    using ShiftID = Int32;
    using SkillID = Int32;
    #endregion

    #region constants
    public static class EnumIterCount
    {
        public const IterCount Max = (1 << 30); // default value for maxIterCount
    }

    public static class EnumNurseID
    {
        public const NurseID None = Begin - 1;
        public const NurseID Begin = default(NurseID);
    }

    public static class EnumShiftID
    {
        public const ShiftID Any = None - 1;
        public const ShiftID None = default(ShiftID);   // the default value makes it no assignment
        public const ShiftID Begin = None + 1;
        public const string AnyStr = "Any";
        public const string NoneStr = "None";
    }

    public static class EnumSkillID
    {
        public const SkillID None = default(SkillID);   // the default value makes it no assignment
        public const SkillID Begin = None + 1;
    }

    public static class EnumContractID
    {
        public const ContractID None = Begin - 1;
        public const ContractID Begin = default(ContractID);
    }

    public static class EnumWeekday
    {
        public const Weekday HIS = 0;
        public const Weekday Mon = HIS + 1;
        public const Weekday Tue = Mon + 1;
        public const Weekday Wed = Tue + 1;
        public const Weekday Thu = Wed + 1;
        public const Weekday Fri = Thu + 1;
        public const Weekday Sat = Fri + 1;
        public const Weekday Sun = Sat + 1;
        public const Weekday NUM = Sun;
        public const Weekday NEXT_WEEK = NUM + 1;
        public const Weekday SIZE = NEXT_WEEK;

        public static readonly string[] weekdayNames = new string[] {
            null, "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
        };

        public static readonly Dictionary<string, Weekday> weekdayMap =
            new Dictionary<string, Weekday> {
            { weekdayNames[Mon], Mon },
            { weekdayNames[Tue], Tue },
            { weekdayNames[Wed], Wed },
            { weekdayNames[Thu], Thu },
            { weekdayNames[Fri], Fri },
            { weekdayNames[Sat], Sat },
            { weekdayNames[Sun], Sun }
        };
    }

    public static class DefaultPenalty
    {
        // (delta >= MAX_OBJ_MAX) stands for forbidden move
        // a tryMove should return FORBIDDEN_MOVE in case it is used by other complex move
        public const ObjValue MAX_OBJ_VALUE = (1 << 24);
        public const ObjValue FORBIDDEN_MOVE = (MAX_OBJ_VALUE * 2);
        // amplifier for improving accuracy
        public const ObjValue AMP = 2 * 2 * 2 * 3 * 7;
        // hard constraints
        public const ObjValue SingleAssign = FORBIDDEN_MOVE;
        public const ObjValue Understaff = FORBIDDEN_MOVE;
        public const ObjValue Succession = FORBIDDEN_MOVE;
        public const ObjValue MissSkill = FORBIDDEN_MOVE;

        public const ObjValue Understaff_Repair = (AMP * 80);
        public const ObjValue Succession_Repair = (AMP * 120);
        // soft constraints
        public const ObjValue InsufficientStaff = (AMP * 30);
        public const ObjValue ConsecutiveShift = (AMP * 15);
        public const ObjValue ConsecutiveDay = (AMP * 30);
        public const ObjValue ConsecutiveDayOff = (AMP * 30);
        public const ObjValue Preference = (AMP * 10);
        public const ObjValue CompleteWeekend = (AMP * 30);
        public const ObjValue TotalAssign = (AMP * 20);
        public const ObjValue TotalWorkingWeekend = (AMP * 30);
    }
    #endregion


    namespace Input
    {
        using System.Runtime.Serialization;
        using System.Runtime.Serialization.Json;


        // original data from instance files
        [DataContract]
        public class INRC2_JsonData
        {
            #region Constructor
            #endregion

            #region Method
            public static INRC2_JsonData ReadInstance(string scenarioPath, string weekdataPath, string initHistoryPath) {
                INRC2_JsonData input = new INRC2_JsonData();

                using (FileStream scenarioStream = File.Open(scenarioPath,
                    FileMode.Open, FileAccess.Read, FileShare.Read)) {
                    input.scenario = DeserializeJsonStream<ScenarioInfo>(scenarioStream);
                }
                using (FileStream weekdataStream = File.Open(weekdataPath,
                    FileMode.Open, FileAccess.Read, FileShare.Read)) {
                    input.weekdata = DeserializeJsonStream<WeekdataInfo>(weekdataStream);
                }
                using (FileStream historyStream = File.Open(initHistoryPath,
                    FileMode.Open, FileAccess.Read, FileShare.Read)) {
                    input.history = DeserializeJsonStream<HistoryInfo>(historyStream);
                }

                return input;
            }

            public static T DeserializeJsonStream<T>(Stream stream) {
                DataContractJsonSerializer js = new DataContractJsonSerializer(typeof(T));
                return (T)js.ReadObject(stream);
            }

            #region file path formatter
            public static string GetScenarioFilePath(string instanceName) {
                return InstanceDir + instanceName + "/Sc-" + instanceName + FileExtension;
            }

            public static string GetWeekdataFilePath(string instanceName, char index) {
                return InstanceDir + instanceName + "/WD-" + instanceName + "-" + index + FileExtension;
            }

            public static string GetInitHistoryFilePath(string instanceName, char index) {
                return InstanceDir + instanceName + "/H0-" + instanceName + "-" + index + FileExtension;
            }

            public static string GetHistoryFilePath(char index) {
                return OutputDir + "/history-week-" + index + FileExtension;
            }
            #endregion
            #endregion

            #region Property
            public ScenarioInfo Scenario {
                get { return scenario; }
                set { scenario = value; }
            }
            public WeekdataInfo Weekdata {
                get { return weekdata; }
                set { weekdata = value; }
            }
            public HistoryInfo History {
                get { return history; }
                set { history = value; }
            }
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
                    public Requirement requirementOnMonday;
                    [DataMember]
                    public Requirement requirementOnTuesday;
                    [DataMember]
                    public Requirement requirementOnWednesday;
                    [DataMember]
                    public Requirement requirementOnThursday;
                    [DataMember]
                    public Requirement requirementOnFriday;
                    [DataMember]
                    public Requirement requirementOnSaturday;
                    [DataMember]
                    public Requirement requirementOnSunday;
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
            private ScenarioInfo scenario;
            [DataMember]
            private WeekdataInfo weekdata;
            [DataMember]
            private HistoryInfo history;
            #endregion
        }

        // formatted structural input data
        [DataContract]
        public class Data
        {
            #region Constructor
            public Data(INRC2_JsonData input) {
                scenario = new ScenarioInfo();
                weekdata = new WeekdataInfo();
                history = new HistoryInfo();
                names = new NameInfo();

                names.shiftMap[EnumShiftID.AnyStr] = EnumShiftID.Any;
                names.shiftMap[EnumShiftID.NoneStr] = EnumShiftID.None;
            }
            #endregion

            #region Method
            // return true if two nurses have same skill.
            // the result is calculated from input data.
            public bool haveSameSkill(NurseID nurse, NurseID nurse2) {
                for (SkillID sk = EnumShiftID.Begin; sk < scenario.skillsLength; ++sk) {
                    if (scenario.nurses[nurse].skills[sk] && scenario.nurses[nurse2].skills[sk]) {
                        return true;
                    }
                }

                return false;
            }

            // do not count min shift number in early weeks
            // max increase with same delta each week.
            void adjustRangeOfTotalAssignByWorkload() {
                for (NurseID nurse = 0; nurse < scenario.nurses.Length; ++nurse) {
                    ScenarioInfo.Contract c = scenario.contracts[scenario.nurses[nurse].contract];
#if INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
                    int weekToStartCountMin = scenario.totalWeekNum * c.minShiftNum;
                    bool ignoreMinShift = ((history.currentWeek * c.maxShiftNum) < weekToStartCountMin);
                    scenario.nurses[nurse].restMinShiftNum = (ignoreMinShift) ? 0 : (c.minShiftNum - history.totalAssignNums[nurse]);
#else
                    scenario.nurses[nurse].restMinShiftNum = c.minShiftNum - history.totalAssignNums[nurse];
#endif
                    scenario.nurses[nurse].restMaxShiftNum = c.maxShiftNum - history.totalAssignNums[nurse];
                    scenario.nurses[nurse].restMaxWorkingWeekendNum = c.maxWorkingWeekendNum - history.totalWorkingWeekendNums[nurse];
                }
            }

            #endregion

            #region Property
            public ScenarioInfo Scenario {
                get { return scenario; }
            }
            public WeekdataInfo Weekdata {
                get { return weekdata; }
            }
            public HistoryInfo History {
                get { return history; }
            }
            public NameInfo Names {
                get { return names; }
            }
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
                    // (legalNextShift[nextShift] == true) means nextShift 
                    // is available to be succession of this shift
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

                    [DataMember]
                    public NurseID[] nurses;            // nurses with this contract
                };

                [DataContract]
                public struct Nurse
                {
                    [DataMember]
                    public ContractID contract;
                    [DataMember]
                    public int restMinShiftNum;         // assignments in the planning horizon
                    [DataMember]
                    public int restMaxShiftNum;         // assignments in the planning horizon
                    [DataMember]
                    public int restMaxWorkingWeekendNum;// assignments in the planning horizon
                    [DataMember]
                    public int skillNum;
                    [DataMember]
                    public bool[] skills;   // (skills[skill] == true) means the nurse have that skill
                };


                [DataMember]
                public int maxWeekCount;    // count from 0
                [DataMember]
                public int totalWeekNum;    // count from 1 (maxWeekCount = (totalWeekNum - 1))

                [DataMember]
                public int skillsLength;    // type number including sentinels
                public int skillTypeNum;    // actual type number

                [DataMember]                // there may be some sentinels in shifts
                public Shift[] shifts;      // shifts.Length is type number including sentinels
                [DataMember]
                public int shiftTypeNum;    // actual type number

                [DataMember]
                public Contract[] contracts;

                [DataMember]
                public Nurse[] nurses;
            }

            [DataContract]
            public struct WeekdataInfo
            {
                // (shiftOffs[day][shift][nurse] == true) means shiftOff required
                [DataMember]
                public bool[, ,] shiftOffs;

                // optNurseNums[day][shift][skill] is a number of nurse
                [DataMember]
                public int[, ,] optNurseNums;

                // minNurseNums[day][shift][skill] is a number of nurse
                [DataMember]
                public int[, ,] minNurseNums;
            }

            [DataContract]
            public struct HistoryInfo
            {
                [DataMember]
                public ObjValue accObjValue;// accumulated objective value
                [DataMember]
                public int pastWeekCount;   // count from 0 (the number in history file)
                [DataMember]
                public int currentWeek;     // count from 1
                [DataMember]
                public int restWeekCount;   // include current week (restWeekCount = (totalWeekNum - pastWeekCount))

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

                [DataMember]
                public string[] skillNames;     // skillMap[skillNames[skillID]] == skillID
                [DataMember]
                public Dictionary<string, SkillID> skillMap;

                [DataMember]
                public string[] shiftNames;     // shiftMap[shiftNames[shiftID]] == shiftID
                [DataMember]
                public Dictionary<string, ShiftID> shiftMap;

                [DataMember]
                public string[] contractNames;  // contractMap[contractNames[contractID]] == contractID
                [DataMember]
                public Dictionary<string, ContractID> contractMap;

                [DataMember]
                public string nurseNames;       // nurseMap[nurseNames[nurseID]] == nurseID
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
    }


    namespace Output
    {
        public class Output
        {
            #region Constructor
            #endregion

            #region Method
            public void LoadAssignTable(string assignStr, int nurseNum, int weekdayNum = EnumWeekday.SIZE) {
                assignTable = new Assign[nurseNum, weekdayNum];

                int i = 0;
                string[] s = assignStr.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
                for (NurseID nurse = EnumNurseID.Begin; nurse < nurseNum; ++nurse) {
                    for (int weekday = EnumWeekday.Mon; weekday <= EnumWeekday.Sun; ++weekday) {
                        assignTable[nurse, weekday] =
                            new Assign(ShiftID.Parse(s[i]), SkillID.Parse(s[i + 1]));
                        i += 2;
                    }
                }
            }
            #endregion

            #region Property
            public Assign[,] AssignTable {
                get { return assignTable; }
                protected set { assignTable = value; }
            }
            #endregion

            #region Type
            public struct Assign
            {
                #region Constructor
                public Assign(ShiftID sh, SkillID sk) {
                    shift = sh;
                    skill = sk;
                }
                #endregion

                #region Method
                public static bool IsWorking(ShiftID shift) {
                    return (shift != EnumShiftID.None);
                }

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
                public bool Working { get { return IsWorking(shift); } }
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
                private ShiftID shift;
                private SkillID skill;
                #endregion
            }
            #endregion

            #region Constant
            #endregion

            #region Field
            // assignTable[nurse][day] is an Assign.
            // weekdayNum should be (actual weekday number + 2) to let it 
            // allocate additional days for history and next week slot.
            private Assign[,] assignTable;
            #endregion
        }

        public class Solution : Output
        {
            #region Constructor
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
    }


    public abstract class Solver
    {
        #region Constructor
        #endregion

        #region Method
        #endregion

        #region Property
        #endregion

        #region Type
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

            // allow hard constraints UnderStaff and Succession being violated
            // but with much greater penalty than soft constraints
            // set softConstraintDecay to MAX_OBJ_VALUE to make them does not count
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
        #endregion
    }

    public class TabuSolver : Solver
    {
        #region Constructor
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
}

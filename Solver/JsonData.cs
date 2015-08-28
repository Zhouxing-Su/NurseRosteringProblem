using System;
using System.Collections.Generic;
using System.Runtime.Serialization;


namespace INRC2
{
    /// <summary> original data from INRC2 instance files. </summary>
    /// <seealso cref="http://mobiz.vives.be/inrc2/"/>
    [DataContract]
    public class JsonData
    {
        #region Constructor
        #endregion Constructor

        #region Method
        public static string getScenarioFilePath(string scenarioName) {
            return InstanceDir + scenarioName + "/Sc-" + scenarioName + FileExtension;
        }

        public static string getWeekdataFilePath(string scenarioName, char index) {
            return InstanceDir + scenarioName + "/WD-" + scenarioName + "-" + index + FileExtension;
        }

        public static string getInitHistoryFilePath(string scenarioName, char index) {
            return InstanceDir + scenarioName + "/H0-" + scenarioName + "-" + index + FileExtension;
        }

        public static string getHistoryFilePath(string outputDir, char index) {
            return outputDir + "history-week-" + index + FileExtension;
        }

        public static int getNurseNumber(string scenario) {
            return Convert.ToInt32(scenario.Substring(1, 3));
        }

        public static int getWeekCount(string scenario) {
            return (scenario[5] - '0');
        }
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

        [DataContract]
        public class SolutionInfo
        {
            [DataContract]
            public class Assignment
            {
                [DataMember]
                public string nurse;
                [DataMember]
                public string day;
                [DataMember]
                public string shiftType;
                [DataMember]
                public string skill;
            }

            [DataMember]
            public string scenario;
            [DataMember]
            public int week;
            [DataMember]
            public List<Assignment> assignments = new List<Assignment>();
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

        private const string FileExtension = @".json";
        private const string InstanceDir = @"Instance/";
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
}

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;


namespace NurseRostering
{
    internal class Program
    {
        public static void Main(string[] args) {
            Util.ArgsProcessor argsProcessor = new Util.ArgsProcessor();
            argsProcessor.process(args, Parameters.Options, Parameters.Switches);

            if (argsProcessor.getSwitchArg(Parameters.HelpSwitch)) {
                Parameters.printHelp();
            }

            Solver.Environment env = loadEnvironment(argsProcessor);

            string path;
            path = argsProcessor.getMappedArg(Parameters.ConfigurePathOption);
            TabuSolver.Configure cfg = (path != null)
                ? Util.readJsonFile<TabuSolver.Configure>(path)
                : new TabuSolver.Configure();

            INRC2.JsonData input = new INRC2.JsonData();
            Problem problem = new Problem();

            input.scenario = Util.readJsonFile<INRC2.JsonData.ScenarioInfo>(
                env.scenarioPath);
            problem.importScenario(input);

            input.weekdata = Util.readJsonFile<INRC2.JsonData.WeekdataInfo>(
                env.weekdataPath);
            problem.importWeekdata(input);

            // load history last for it will initialize some assist data depending on scenario and weekdata
            if (env.customInputPath != null) {
                problem.readCustomFile(env.customInputPath);
            } else {
                input.history = Util.readJsonFile<INRC2.JsonData.HistoryInfo>(
                    env.historyPath);
                problem.importHistory(input);
            }

            TabuSolver solver = new TabuSolver(problem, env, cfg);
            solver.solve();

            Util.writeJsonFile(env.solutionPath, problem.exportSolution(solver.Optima));
            if (env.customOutputPath != null) {
                Problem.HistoryInfo newHistory;
                solver.generateHistory(out newHistory);
                problem.writeCustomFile(env.customOutputPath, ref newHistory);
            }

            path = argsProcessor.getMappedArg(Parameters.LogPathOption);
            if (path != null) {
                solver.print();
                solver.record(path);
            }
        }

        public static Solver.Environment loadEnvironment(Util.ArgsProcessor argsProcessor) {
            string str;

            str = argsProcessor.getMappedArg(Parameters.EnvironmentPathOption);
            Solver.Environment env = ((str != null)
                ? (env = Util.readJsonFile<Solver.Environment>(str))
                : (new Solver.Environment()));

            str = argsProcessor.getMappedArg(Parameters.ScenarioPathOption);
            if (str != null) { env.scenarioPath = str; }
            str = argsProcessor.getMappedArg(Parameters.WeekdataPathOption);
            if (str != null) { env.weekdataPath = str; }
            str = argsProcessor.getMappedArg(Parameters.HistoryPathOption);
            if (str != null) { env.historyPath = str; }
            str = argsProcessor.getMappedArg(Parameters.SolutionPathOption);
            if (str != null) { env.solutionPath = str; }
            str = argsProcessor.getMappedArg(Parameters.CustomInputPathOption);
            if (str != null) { env.customInputPath = str; }
            str = argsProcessor.getMappedArg(Parameters.CustomOutputPathOption);
            if (str != null) { env.customOutputPath = str; }

            str = argsProcessor.getMappedArg(Parameters.RandomSeedOption);
            if (str != null) { env.randSeed = Convert.ToInt32(str); }
            str = argsProcessor.getMappedArg(Parameters.TimeoutOption);
            if (str != null) { env.timeoutInSeconds = Convert.ToDouble(str); }
            str = argsProcessor.getMappedArg(Parameters.MaxIterationOption);
            if (str != null) { env.maxIterCount = Convert.ToInt32(str); }

            str = argsProcessor.getMappedArg(Parameters.IndstanceInfoOption);
            if (str != null) { env.instanceInfo = str; }

            str = argsProcessor.getMappedArg(Parameters.IdOption);
            if (str != null) { env.id = str; }

            return env;
        }

        public static class Parameters
        {
            #region options
            public const string ScenarioPathOption = "-sce";
            public const string HistoryPathOption = "-his";
            public const string WeekdataPathOption = "-week";
            public const string SolutionPathOption = "-sol";
            public const string CustomInputPathOption = "-cusIn";
            public const string CustomOutputPathOption = "-cusOut";
            public const string RandomSeedOption = "-rand";
            public const string TimeoutOption = "-timeout";
            public const string MaxIterationOption = "-iter";
            public const string IdOption = "-id";
            public const string IndstanceInfoOption = "-inst";
            public const string EnvironmentPathOption = "-env";
            public const string ConfigurePathOption = "-cfg";
            public const string LogPathOption = "-log";

            public static readonly string[] Options = {
                ScenarioPathOption,
                HistoryPathOption,
                WeekdataPathOption,
                SolutionPathOption,
                CustomInputPathOption,
                CustomOutputPathOption,
                RandomSeedOption,
                TimeoutOption,
                MaxIterationOption,
                IdOption,
                IndstanceInfoOption,
                EnvironmentPathOption,
                ConfigurePathOption,
                LogPathOption
            };
            #endregion options

            #region switches
            public const string HelpSwitch = "-h";

            public static readonly string[] Switches = {
                HelpSwitch
            };
            #endregion switches

            public const string HelpString =
                "Arguments usage ([] means optional) :\n"
                + "  -sce       scenario file path.\n"
                + "  -his       history file path.\n"
                + "  -week      weekdata file path.\n"
                + "  -sol       solution file path.\n"
                + "  [-cusIn]   custom input file path.\n"
                + "  [-cusOut]  custom output file path.\n"
                + "  [-rand]    rand seed for the solver.\n"
                + "  [-timeout] max running time of the solver.\n"
                + "  [-iter]    max iteration count of the solver.\n"
                + "  [-id]      distinguish different runs in log file and output.\n"
                + "  [-inst]    brief information of instance for logging.\n"
                + "  [-env]     environment file path.\n"
                + "  [-cfg]     configure file path.\n"
                + "  [-log]     activate logging and specify log file path.\n"
                + "  [-h]       print help information.\n"
                + "Note:\n"
                + "  1. an environment file contains information of all options\n"
                + "     except -cfg and -log. it may be override by options.\n"
                + "  2. reaching either timeout or iter will stop the solver.\n"
                + "     if you specify neither of them, the solver will be running\n"
                + "     for a long time. so you should set at least one of them.\n"
                + "  3. if custom input file is given, the history file is ignored.";

            public static void printHelp() {
                Console.WriteLine(Parameters.HelpString);
            }
        }
    }
}

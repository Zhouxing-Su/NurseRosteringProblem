using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;


namespace NurseRostering
{
    internal class Program
    {
        public static void help() {
            Console.WriteLine(
                "command line arguments ([XXX] means XXX is optional) :\n"
                + "  sce       - scenario file path.\n"
                + "  his       - history file path.\n"
                + "  week      - weekdata file path.\n"
                + "  sol       - solution file path.\n"
                + "  [cusIn]   - custom input file path.\n"
                + "  [cusOut]  - custom output file path.\n"
                + "  [rand]    - rand seed for the solver.\n"
                + "  [timeout] - max running time of the solver.\n"
                + "  [iter]    - max iteration count of the solver.\n"
                + "              reaching either timeout or iter will stop the solver.\n"
                + "  [id]      - identifier of the run to be recorded in log file.\n"
                + "  [inst]    - brief information of instance for logging.\n"
                + "  [env]     - environment file path.\n"
                + "  [cfg]     - configure file path.\n");
        }

        public static void Main(string[] args) {
            string instName = INRC2JsonData.Instance.n005w4.ToString();
            char weekdataIndex = '4';
            char initHistoryIndex = '1';
            INRC2JsonData input = new INRC2JsonData();

            input.scenario = Util.readJsonFile<INRC2JsonData.ScenarioInfo>(
                INRC2JsonData.getScenarioFilePath(instName));
            input.weekdata = Util.readJsonFile<INRC2JsonData.WeekdataInfo>(
                INRC2JsonData.getWeekdataFilePath(instName, weekdataIndex));
            input.history = Util.readJsonFile<INRC2JsonData.HistoryInfo>(
                INRC2JsonData.getInitHistoryFilePath(instName, initHistoryIndex));

            Problem problem = new Problem();
            problem.importScenario(input);
            problem.importWeekdata(input);
            problem.importHistory(input);

            string envPath = "environment.json";
            string cfgPath = "configure.json";
            //Solver.Environment env = new Solver.Environment();
            //env.instanceInfo = instName + "[W" + weekdataIndex + "][H0[" + initHistoryIndex + "]]";
            //File.WriteAllText(envPath, env.toJsonString());
            //TabuSolver.Configure config = new TabuSolver.Configure();
            //File.WriteAllText(cfgPath, config.toJsonString());

            Solver.Environment env = Util.readJsonFile<Solver.Environment>(envPath);
            TabuSolver.Configure config = Util.readJsonFile<TabuSolver.Configure>(cfgPath);

            string logPath = "log.csv";
            string outputDir = "Output/";
            string slnPath = outputDir + "sln0.json";
            Directory.CreateDirectory(outputDir);
            TabuSolver solver = new TabuSolver(problem, env, config);
            solver.solve();
            solver.print();
            solver.record(logPath);
            Util.writeJsonFile(slnPath, problem.exportSolution(solver.Optima));
        }
    }
}

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;


namespace NurseRostering
{
    internal class Program
    {
        public static void Main(string[] args) {
            string instName = INRC2JsonData.Instance.n030w4.ToString();
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

            //TabuSolver.Configure config = new TabuSolver.Configure(0);
            string configPath = "config.json";
            //File.WriteAllText(configPath, TabuSolver.Configure.getDefaultConfigString());
            TabuSolver.Configure config = Util.readJsonFile<TabuSolver.Configure>(configPath);
            config.instanceName = instName + "[W" + weekdataIndex + "][H0[" + initHistoryIndex + "]]";

            string logPath = "log.csv";
            string outputDir = "Output/";
            string slnPath = outputDir + "sln0.json";
            Directory.CreateDirectory(outputDir);
            for (int i = 0; i < 1000; i++) {
                TabuSolver solver = new TabuSolver(problem, config);
                solver.solve();
                solver.print();
                solver.record(logPath);
                Util.writeJsonFile(slnPath, problem.exportSolution(solver.Optima));
            }
        }
    }
}

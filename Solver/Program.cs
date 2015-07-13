using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.Serialization.Json;
using System.IO;


namespace NurseRostering
{
    class Program
    {
        static void Main(string[] args) {
            string instName = Input_INRC2Json.Instance.n005w4.ToString();
            char weekdataIndex = '4';
            char initHistoryIndex = '1';
            Input_INRC2Json i = new Input_INRC2Json();

            i.scenario = Input_INRC2Json.readFile<Input_INRC2Json.ScenarioInfo>(
                Input_INRC2Json.getScenarioFilePath(instName));
            i.weekdata = Input_INRC2Json.readFile<Input_INRC2Json.WeekdataInfo>(
                Input_INRC2Json.getWeekdataFilePath(instName, weekdataIndex));
            i.history = Input_INRC2Json.readFile<Input_INRC2Json.HistoryInfo>(
                Input_INRC2Json.getInitHistoryFilePath(instName, initHistoryIndex));

            Solver.Input d = new Solver.Input();
            d.loadScenario(i);
            d.loadWeekdata(i);
            d.loadHistory(i);

            //System.Console.WriteLine(Stopwatch.Frequency);
            //Stopwatch sw = new Stopwatch();
            //sw.Start();

            //sw.Stop();
            //Console.WriteLine(sw.ElapsedTicks);
        }
    }
}

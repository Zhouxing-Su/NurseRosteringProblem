using System;
using System.Threading;
using System.Diagnostics;


namespace NurseRostering
{
    class Program
    {
        static void Main(string[] args) {
            string instName = Input.INRC2_JsonData.Instance.n005w4.ToString();
            char weekdataIndex = '4';
            char initHistoryIndex = '1';
            Input.INRC2_JsonData i = Input.INRC2_JsonData.ReadInstance(
                Input.INRC2_JsonData.GetScenarioFilePath(instName),
                Input.INRC2_JsonData.GetWeekdataFilePath(instName, weekdataIndex),
                Input.INRC2_JsonData.GetInitHistoryFilePath(instName, initHistoryIndex));

            Input.Data d = new Input.Data(i);

            //System.Console.WriteLine(Stopwatch.Frequency);
            //Stopwatch sw = new Stopwatch();
            //sw.Start();

            //sw.Stop();
            //Console.WriteLine(sw.ElapsedTicks);
        }
    }
}

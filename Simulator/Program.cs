using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace INRC2
{
    using NurseRostering;

    internal class Program
    {
        static void Main(string[] args) {
            Directory.CreateDirectory(OutputDir);

            string instName = INRC2.JsonData.Instance.n005w4.ToString();
            char weekdataIndex = '4';
            char initHistoryIndex = '1';

            string envPath = "environment.json";
            string cfgPath = "configure.json";
            Solver.Environment env = new Solver.Environment();
            env.instanceInfo = instName + "[W" + weekdataIndex + "][H0[" + initHistoryIndex + "]]";
            File.WriteAllText(envPath, env.toJsonString());
            TabuSolver.Configure config = new TabuSolver.Configure();
            File.WriteAllText(cfgPath, config.toJsonString());

            string slnPath = OutputDir + "sln0.json";

            NurseRostering.Program.Main(null);
        }

        public const string OutputDir = "Output/";
        public const string LogPath = "log.csv";
    }
}

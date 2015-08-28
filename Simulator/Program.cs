using System;


namespace INRC2
{
    using NurseRostering;


    internal class Program
    {
        public static void Main(string[] args) {
            Util.waitTerminationCodeAsync("szx");

            Simulator simulator = new Simulator(
                Util.readJsonFile<Misson>(Simulator.DefaultMissionPath));

            simulator.launch();

            //Util.waitTerminationCode("szx");zd
        }
    }
}

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace NurseRostering
{
    public static class Util
    {
        public static byte[] GetBytes(string str) {
            return Encoding.GetEncoding(str).GetBytes(str);
        }
    }
}

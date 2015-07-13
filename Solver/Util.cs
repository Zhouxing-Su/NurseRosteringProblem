using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace NurseRostering
{
    public static class Util
    {
        /// <summary> define deep copy </summary>
        public interface ICopyable<T>
        {
            void copyTo(T destination);
        }

        public static T[] CreateArray<T>(int length, T initValue) where T : struct {
            T[] array = new T[length];
            for (int i = 0; i < length; ++i) {
                array[i] = initValue;
            }
            return array;
        }

        public static T[] SetAll<T>(this T[] array, T value) where T : struct {
            for (int i = 0; i < array.Length; ++i) {
                array[i] = value;
            }

            return array;
        }

        public static byte[] GetBytes(string str) {
            return Encoding.GetEncoding(str).GetBytes(str);
        }
    }
}

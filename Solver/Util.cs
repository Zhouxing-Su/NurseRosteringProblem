using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
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

        public static class Worker
        {
            /// <summary> run method in synchronized way with a timeout. </summary>
            /// <returns> true if the work is finished within timeout. </returns>
            public static bool WorkUntilTimeout(
                ThreadStart work,
                int millisecondsTimeout = Timeout.Infinite) {
                Thread thread = new Thread(work);
                thread.Start();
                if (!thread.Join(millisecondsTimeout)) {
                    thread.Abort();
                    return false;
                }
                return true;
            }

            public static bool WorkUntilTimeout(
                ParameterizedThreadStart work, object workArg,
                int millisecondsTimeout = Timeout.Infinite) {
                Thread thread = new Thread(work);
                thread.Start(workArg);
                if (!thread.Join(millisecondsTimeout)) {
                    thread.Abort();
                    return false;
                }
                return true;
            }

            public static bool WorkUntilTimeout(
                Action work,
                Action onAbort,
                int millisecondsTimeout = Timeout.Infinite) {
                Thread thread = new Thread(() => {
                    try {
                        work();
                    } catch (ThreadAbortException) {
                        onAbort();
                    }
                });
                thread.Start();
                if (!thread.Join(millisecondsTimeout)) {
                    thread.Abort();
                    return false;
                }
                return true;
            }

            public static bool WorkUntilTimeout(
                Action<object> work, object workArg,
                Action onAbort,
                int millisecondsTimeout = Timeout.Infinite) {
                Thread thread = new Thread((object obj) => {
                    try {
                        work(obj);
                    } catch (ThreadAbortException) {
                        onAbort();
                    }
                });
                thread.Start(workArg);
                if (!thread.Join(millisecondsTimeout)) {
                    thread.Abort();
                    return false;
                }
                return true;
            }
        }

        public static void Swap<T>(ref T lhs, ref T rhs) {
            T temp = lhs;
            lhs = rhs;
            rhs = temp;
        }

        // -------+--------+------
        //       low      high
        //
        // if target is less than low, the distance is (low - target),
        // if target is greater than high, the distance is (target - high)
        // else, the distance is 0
        // 
        // require operator-() and operator<()
        public static int distanceToRange(int target, int lowerBound, int upperBound) {
            int dist = lowerBound - target;
            if (dist < 0) {
                dist = target - upperBound;
                if (dist < 0) {
                    dist = 0;
                }
            }
            return dist;
        }

        // ---------+-----------
        //        bound---->
        //
        // if target is greater than bound, the distance is (target - bound)
        // else, the distance is 0
        // 
        // require operator-() and operator<()
        public static int exceedCount(int target, int bound) {
            int diff = target - bound;
            return (0 < diff) ? diff : 0;
        }

        // ---------+-----------
        //   <----bound
        //
        // if target is less than bound, the distance is (bound - target)
        // else, the distance is 0
        // 
        // require operator-() and operator<()
        public static int absentCount(int target, int bound) {
            int diff = bound - target;
            return (0 < diff) ? diff : 0;
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

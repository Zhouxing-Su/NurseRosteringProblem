using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading;


namespace NurseRostering
{
    public static class Util
    {
        /// <summary> define deep copy </summary>
        public interface ICopyable<T>
        {
            void copyTo(T destination);
        }

        public static void printLog(string msg) {
            Console.Error.Write(DateTime.Now.ToString(Durations.TimeFormat_Readable));
            Console.Error.WriteLine("," + msg);
        }

        public static void waitTerminationCodeAsync(string code) {
            Thread waitTermination = new Thread(() => {
                while (Console.ReadLine() != code) { }
                Environment.Exit(0);
            });
            waitTermination.IsBackground = true;
            waitTermination.Start();
        }

        public static void waitTerminationCode(string code) {
            Thread.CurrentThread.IsBackground = true;
            while (Console.ReadLine() != code) { }
            Environment.Exit(0);
        }

        /// <summary>
        /// it can handles 3 types of arguments.                               <para />
        /// the first one is plain arguments, which come by themselves.        <para />
        /// the second one is map arguments, which come as key value pairs.
        ///     (the keys are predefined)                                      <para />
        /// the third one is switch arguments, which come by themselves.
        ///     (the values are predefined)
        /// </summary>
        public class ArgsProcessor
        {
            #region Constructor
            #endregion Constructor

            #region Method
            public void process(string[] args, string[] options, string[] switches) {
                foreach (string item in options) { mapArgs.Add(item, null); }
                HashSet<string> switchSet = new HashSet<string>(switches);

                for (int i = 0; i < args.Length; i++) {
                    if (mapArgs.ContainsKey(args[i])) {
                        mapArgs[args[i]] = args[i + 1];
                        i++;
                    } else if (switchSet.Contains(args[i])) {
                        switchArgs.Add(args[i]);
                    } else {
                        plainArgs.Add(args[i]);
                    }
                }
            }

            /// <summary> get a plain argument by order of appeared plain args. </summary>
            public string getPlainArg(int argIndex) {
                return plainArgs[argIndex];
            }

            /// <summary> get a map argument by key. </summary>
            public string getMappedArg(string optionName) {
                return mapArgs[optionName];
            }

            /// <summary> if the switch is specified, return true. </summary>
            public bool getSwitchArg(string switchName) {
                return switchArgs.Contains(switchName);
            }
            #endregion Method

            #region Property
            #endregion Property

            #region Type
            #endregion Type

            #region Constant
            #endregion Constant

            #region Field
            /// <summary> if the arg does not exist, the value will be null. </summary>
            private Dictionary<string, string> mapArgs = new Dictionary<string, string>();
            /// <summary> stores arguments by order of appeared plain args. </summary>
            private List<string> plainArgs = new List<string>();
            /// <summary> if there is a certain switch, the related item exists in switchArgs. </summary>
            private HashSet<string> switchArgs = new HashSet<string>();
            #endregion Field
        }

        public static class IntID
        {
            public static int generate() { return Interlocked.Increment(ref id); }

            private static int id = 0;
        }

        public static int genRandSeed() {
            return (Environment.TickCount + Environment.CurrentManagedThreadId);
        }

        public static void writeJsonFile<T>(string path, T obj) {
            using (FileStream fs = File.Open(path,
                FileMode.Create, FileAccess.Write, FileShare.Read)) {
                serializeJsonStream<T>(fs, obj);
            }
        }

        public static void serializeJsonStream<T>(Stream stream, T obj) {
            DataContractJsonSerializer js = new DataContractJsonSerializer(typeof(T));
            js.WriteObject(stream, obj);
        }

        public static T readJsonFile<T>(string path) {
            using (FileStream fs = File.Open(path,
                FileMode.Open, FileAccess.Read, FileShare.Read)) {
                return deserializeJsonStream<T>(fs);
            }
        }

        public static T deserializeJsonStream<T>(Stream stream) {
            DataContractJsonSerializer js = new DataContractJsonSerializer(typeof(T));
            return (T)js.ReadObject(stream);
        }

        public static string toJsonString<T>(this T obj) {
            using (MemoryStream ms = new MemoryStream()) {
                serializeJsonStream(ms, obj);
                ms.Position = 0;
                using (StreamReader sr = new StreamReader(ms)) {
                    return sr.ReadToEnd();
                }
            }
        }

        public static class Worker
        {
            /// <summary> run method in synchronized way with a non-negative timeout. </summary>
            /// <returns> true if the work is finished within timeout. </returns>
            /// <remarks> 
            /// Timeout.Infinite is not supported (you should call your method directly). <para />
            /// use lambda expression to wrap the parameterized methods. 
            /// </remarks>
            public static bool WorkUntilTimeout(ThreadStart work,
                int millisecondsTimeout) {
                if (millisecondsTimeout < 0) { return false; }
                Thread thread = new Thread(work);
                thread.Start();
                if (!thread.Join(millisecondsTimeout)) {
                    thread.Abort();
                    return false;
                }
                return true;
            }

            public static bool WorkUntilTimeout(Action work, Action onAbort,
                int millisecondsTimeout) {
                if (millisecondsTimeout < 0) {
                    onAbort();
                    return false;
                }
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
                Action work, Action onAbort, Action onExit,
                int millisecondsTimeout) {
                if (millisecondsTimeout < 0) {
                    onAbort();
                    onExit();
                    return false;
                }
                Thread thread = new Thread(() => {
                    try {
                        work();
                    } catch (ThreadAbortException) {
                        onAbort();
                    } finally {
                        onExit();
                    }
                });
                thread.Start();
                if (!thread.Join(millisecondsTimeout)) {
                    thread.Abort();
                    return false;
                }
                return true;
            }
        }

        public class RandSelect
        {
            /// <summary>
            /// sometimes the first element is pre-selected with the possibility 
            /// of 1, so you can pass 1 into the constructor in this condition to 
            /// leave out a isSelected() call.
            /// </summary>
            public RandSelect(int startCount = 1) {
                count = startCount;
            }

            /// <summary> 
            /// start a new selection on another N elements.
            /// sometimes the first element is pre-selected with the possibility of 1, 
            /// so you can pass 1 in this condition to leave out a isSelected() call.
            /// </summary>
            public void reset(int startCount = 1) {
                count = startCount;
            }

            /// <summary>
            /// call this for each of the N elements (N times in total) to judge 
            /// whether each of them is selected. 
            /// only the last returned "true" means that element is selected finally.
            /// </summary>
            public bool isSelected(int randNum) {
                return ((randNum % (++count)) == 0);
            }

            public bool isMinimal(int target, int min, int randNum) {
                if (target < min) {
                    reset();
                    return true;
                } else if (target == min) {
                    return isSelected(randNum);
                } else {
                    return false;
                }
            }
            public bool isMinimal(long target, long min, int randNum) {
                if (target < min) {
                    reset();
                    return true;
                } else if (target == min) {
                    return isSelected(randNum);
                } else {
                    return false;
                }
            }
            public bool isMinimal(double target, double min, int randNum) {
                if (target < min) {
                    reset();
                    return true;
                } else if (target == min) {
                    return isSelected(randNum);
                } else {
                    return false;
                }
            }

            private int count;
        }

        public static void fill<T>(this IList<T> list, T value) {
            for (int i = 0; i < list.Count; i++) {
                list[i] = value;
            }
        }
        public static void fill<T>(this IList<T> list, T value, int offset, int length) {
            for (int i = offset; i < length; i++) {
                list[i] = value;
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

#include <iostream>
#include <fstream>
#include <vector>
#include <string>


using namespace std;


int main() {
    string prefix("D:/workspace/NRP/");
    vector<string> machine = {
        "0", "1"
    };

    vector<string> alg = {
        "/BTS_WITHOUTSWAP/", "/BTS_WITHSWAP/"
    };

    vector<string> thread = {
        "0","1","2","3","4","5","6","7"
    };

    vector<string> inst = {
        ".n035w4.", ".n070w4.", ".n110w4."
    };

    vector<string> run = {
        "1.", "2."
    };

    vector<string> metric = {
        "0iter.csv", "0time.csv"
    };


    int filenum = machine.size() * thread.size() * run.size();

    ofstream ofs("out.txt");
    for (int i6 = 0; i6 < metric.size(); ++i6) {
        for (int i2 = 0; i2 < alg.size(); ++i2) {
            for (int i4 = 0; i4 < inst.size(); ++i4) {
                vector<double> v(125, 0);
                for (int i1 = 0; i1 < machine.size(); ++i1) {
                    for (int i3 = 0; i3 < thread.size(); ++i3) {
                        for (int i5 = 0; i5 < run.size(); ++i5) {
                            string fn(prefix + machine[i1] + alg[i2] + thread[i3] + inst[i4] + run[i5] + metric[i6]);
                            ifstream ifs(fn);

                            double d;
                            for (int i = 0; i < 125; ++i) {
                                ifs >> d;
                                v[i] += (d / filenum);
                            }

                            ifs.close();
                        }
                    }
                }

                int k = 0;
                for (size_t i = 0; i < 25; ++i) {
                    for (size_t j = 0; j < 5; ++j, ++k) {
                        ofs << "(" << k << "," << v[k] << ") ";
                    }
                    ofs << endl;
                }
                ofs << endl;

            }
        }
    }
    ofs.close();

    return 0;
}
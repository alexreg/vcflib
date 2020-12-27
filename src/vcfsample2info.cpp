/*
    vcflib C++ library for parsing and manipulating VCF files

    Copyright © 2010-2020 Erik Garrison
    Copyright © 2020      Pjotr Prins

    This software is published under the MIT License. See the LICENSE file.
*/

#include "Variant.h"
#include "split.h"
#include "Fasta.h"
#include <getopt.h>
#include <algorithm>
#include <numeric>

using namespace std;
using namespace vcflib;

void printSummary(char** argv) {
    cerr << "usage: " << argv[0] << " [options] <vcf file>" << endl
         << endl
         << "options:" << endl
         << "    -f, --field         Add information about this field in samples to INFO column" << endl
         << "    -i, --info          Store the computed statistic in this info field" << endl
         << "    -a, --average       Take the mean of samples for field (default)" << endl
         << "    -m, --median        Use the median" << endl
         << "    -n, --min           Use the min" << endl
         << "    -x, --max           Use the max" << endl
         << endl
         << "Take annotations given in the per-sample fields and add the mean, median, min, or max" << endl
         << "to the site-level INFO." << endl
         << endl;
    cerr << endl << "Type: transformation" << endl << endl;
    exit(0);
}

double median(vector<double> &v)
{
    size_t n = v.size() / 2;
    nth_element(v.begin(), v.begin()+n, v.end());
    return v[n];
}

double mean(vector<double> &v)
{
    double sum = accumulate(v.begin(), v.end(), 0.0);
    return sum / v.size();
}

enum StatType { MEAN, MEDIAN, MIN, MAX };

int main(int argc, char** argv) {

    int c;
    string sampleField;
    string infoField;
    StatType statType = MEAN;

    if (argc == 1)
        printSummary(argv);

    while (true) {
        static struct option long_options[] =
            {
                /* These options set a flag. */
                {"help", no_argument, 0, 'h'},
                {"field",  required_argument, 0, 'f'},
                {"info",  required_argument, 0, 'i'},
                {"average", no_argument, 0, 'a'},
                {"median", no_argument, 0, 'm'},
                {"min", no_argument, 0, 'n'},
                {"max", no_argument, 0, 'x'},
                {0, 0, 0, 0}
            };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "hamnxf:i:",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
                break;
            printf ("option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;

        case 'f':
            sampleField = optarg;
            break;

        case 'i':
            infoField = optarg;
            break;

        case 'a':
            statType = MEAN;
            break;

        case 'm':
            statType = MEDIAN;
            break;

        case 'n':
            statType = MIN;
            break;

        case 'x':
            statType = MAX;
            break;

        case 'h':
            printSummary(argv);
            exit(0);

        case '?':
            /* getopt_long already printed an error message. */
            printSummary(argv);
            exit(1);
            break;

        default:
            abort ();
        }
    }

    if (infoField.empty() || sampleField.empty()) {
        cerr << "Error: both a sample field and an info field are required." << endl;
        return 1;
    }

    VariantCallFile variantFile;
    string inputFilename;
    if (optind == argc - 1) {
        inputFilename = argv[optind];
        variantFile.open(inputFilename);
    } else {
        variantFile.open(std::cin);
    }

    if (!variantFile.is_open()) {
        return 1;
    }

    string statTypeStr;

    switch (statType) {
    case MEAN:
        statTypeStr = "mean";
        break;
    case MEDIAN:
        statTypeStr = "median";
        break;
    case MIN:
        statTypeStr = "min";
        break;
    case MAX:
        statTypeStr = "max";
        break;
    default:
        cerr << "Error: failure to convert stat type to string" << endl;
        return 1;
        break;
    }

    variantFile.addHeaderLine("##INFO=<ID="+infoField+",Number=1,Type=Float,Description=\"Summary statistic generated by"+statTypeStr+" of per-sample values of "+sampleField+" \">");

    cout << variantFile.header << endl;

    Variant var(variantFile);
    while (variantFile.getNextVariant(var)) {
        vector<double> vals;
        for (map<string, map<string, vector<string> > >::iterator s = var.samples.begin();
             s != var.samples.end(); ++s) {
            map<string, vector<string> >& sample = s->second;
            if (sample.find(sampleField) != sample.end()) {
                double val;
                string& s = sample[sampleField].front();
                if (sample[sampleField].size() > 1) {
                    cerr << "Error: cannot handle sample fields with multiple values" << endl;
                    return 1;
                }
                convert(s, val);
                vals.push_back(val);
            }
        }

        double result;
        switch (statType) {
        case MEAN:
            result = mean(vals);
            break;
        case MEDIAN:
            result = median(vals);
            break;
        case MIN:
            result = *min_element(vals.begin(), vals.end());
            break;
        case MAX:
            result = *max_element(vals.begin(), vals.end());
            break;
        default:
            cerr << "Error: unrecognized StatType" << endl;
            return 1;
            break;
        }

        var.info[infoField].clear();
        var.info[infoField].push_back(convert(result));

        cout << var << endl;

    }

    return 0;

}

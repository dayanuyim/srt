//#include <UnitTest++.h>
#include "srt.h"

#include <iostream>
#include <string>
#include <list>
#include <exception>
#include <chrono>
#include <cctype>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::ostream;
using std::istream;
using std::stringstream;
using std::string;
using std::list;
using std::exception;
using boost::regex;
using boost::posix_time::ptime;
using boost::posix_time::time_duration;
using boost::posix_time::duration_from_string;

string getHelp()
{
    stringstream ss;

    ss << "USAGE" << endl;
    ss << " srt [options] [command] [arguments]" << endl;
    ss << endl;
    ss << "OPTIONS" << endl;
    ss << "  -c" << endl;
    ss << "    condense multile lines to one line." << endl;
    ss << "  -f=[uwm]" << endl;
    ss << "    the newline of output: unix, windows, or mac" << endl;
    ss << endl;
    ss << "COMMANDS" << endl;
    ss << "  merge  Merge multiple N srt files. N >= 1" << endl;
    ss << "    merge <file 1>...<file N>." << endl;
    ss << endl;
    ss << "  offset  Offset a relative time." << endl;
    ss << "    offset +/-<time> <file>" << endl;
    ss << "      <time> = 'HH:mm:ss,fff'" << endl;
    ss << "    offset -<n> <time> <file>" << endl;
    ss << "      <n> is srt sn. The form is like above, but using specified time." << endl;
    ss << endl;
    ss << "  sync  Synchonize to the specified time(s)." << endl;
    ss << "    sync -<n1> <time1> -<n2> <time2> <file>." << endl;
    ss << "      <n1>, <n2> is srt sn. if <time> prefix '+' or '-', it is a offset, otherwise a specified time." << endl;
    return ss.str();
}

class ArgError: public std::exception{
public:
    ArgError(const string &msg)
        :msg_(msg){}

    virtual const char* what() const noexcept{
        return msg_.c_str();
    }

private:
    std::string msg_;
};

void dispatchMerge(const SrtOpt &opt, const list<const char*> &args)
{
    if(args.empty())
        throw ArgError("no file to merge");

    cerr << "merge " << args.size() << " files..." << endl;

    auto arg = args.cbegin(), arg_end = args.cend();

    // merge
    Srt merged{*arg++, opt};
    for(; arg != arg_end; ++arg)
        merged += Srt{*arg, opt};

    const time_duration min_len = boost::posix_time::milliseconds(100); 
    merged.filter([&](const Srt::Item &item) -> bool{ return item.period.length() >= min_len; });
    cout << merged;
}

const string& getTimeRegexStr()
{
    static string str;
    if(str.empty()){
        const char *hour = "\\d+";
        const char *saxag = "[012345]?\\d";
        const char *frag = "(,\\d{3})?";
        stringstream ss;
        ss << hour << ':' << saxag << ':' << saxag << frag;
        str = ss.str();
    }
    return str;
}

int getSyncSn(const char *arg)
{
    // cppcheck-suppress constStatement
    static regex is_idx{"-\\d+"};

    if(!regex_match(arg, is_idx))
        throw ArgError("invalid item idx");
    return abs(atoi(arg));
}

inline
ptime toSpecTime(const char *str)
{
    static regex is_time{getTimeRegexStr()};

    if(!regex_match(str, is_time))
        throw ArgError("invalid item time");

    return Srt::toTime(str);
}

time_duration toOffsetTime(const char *str)
{
    // cppcheck-suppress constStatement
    static regex is_time{"[+-]" + getTimeRegexStr()};

    if(!regex_match(str, is_time)){
        cerr << "str: [" << str << "]" << endl;
        throw ArgError("time format error");
    }

    return duration_from_string(str);
}

double getScale(ptime org_time1, ptime org_time2, ptime time1, ptime time2)
{
    //span
    long org_span = (org_time2 - org_time1).total_milliseconds();
    long span = (time2 - time1).total_milliseconds();
    if(org_span == 0)
        throw ArgError("invalid sync span");

    //scale
    return (double)span/org_span;
}

void dispatchOffset(const SrtOpt &opt, list<const char*> &args)
{
    //args
    //form 1: offset-time srt-file
    //form 2: -sn specified-time srt-file

	if(args.size() != 2 && args.size() != 3)
        throw ArgError("bad argument");

	const bool is_offset = (args.size() == 2);

	//srt
	Srt srt{args.back(), opt};
	args.pop_back();

	//offset
	time_duration offset = is_offset?
		toOffsetTime(args.front()):
		toSpecTime(args.back()) - srt[getSyncSn(args.front())].period.begin();

	srt.offset(offset);
	
	//output
	cout << srt;
}

inline
bool isOffsetTime(const char *str)
{
    return (*str == '+') || (*str == '-');
}

ptime parseTime(const char *arg, const Srt &srt, int sn)
{
    return isOffsetTime(arg)?
        srt[sn].period.begin() + toOffsetTime(arg) :
        toSpecTime(arg);
}

void syncSrt(Srt &srt, int sn1, const ptime &time1, int sn2, const ptime &time2)
{
    //scale
    double scale = getScale( 
            srt[sn1].period.begin(),
            srt[sn2].period.begin(),
            time1,
            time2);
    srt.scale(scale);

    //offset
    time_duration offset = time1 - srt[sn1].period.begin();
    srt.offset(offset);
}

void dispatchSync(const SrtOpt &opt, const list<const char*> &args)
{
    // arguments: -sn1 time1 -sn2 time2 srtfile
    if(args.size() != 5)
        throw ArgError("bad sync arguments");

    //srt
    Srt srt{args.back(), opt};

    auto arg = args.begin();

    //get 1st sn,time pair
    int sn1 = getSyncSn(*arg);
    ++arg;
    ptime time1 = parseTime(*arg, srt, sn1);
    ++arg;

    //get 2nd sn,time pair
    int sn2 = getSyncSn(*arg);
    ++arg;
    ptime time2 = parseTime(*arg, srt, sn2);
    ++arg;

    // sync, then print out
    syncSrt(srt, sn1, time1, sn2, time2);
    cout << srt;
}

//read out options from args,
//return SrtOpt
SrtOpt readOutOpt(list<const char*> &args)
{
	SrtOpt opt;

    for(auto it = args.begin(); it != args.end();){

		if(strcmp(*it, "-c") == 0){
			opt.is_condense = true;
            it = args.erase(it);
        }
        else if(strncmp(*it, "-f=", 3) == 0){
            char ch = (*it)[3];
            opt.newline = 
                (ch == 'u')? "\n":
                (ch == 'w')? "\r\n":
                (ch == 'm')? "\r":
                throw ArgError("invalid new line format");

            it = args.erase(it);
        }
        else
            ++it;
	}

    return opt;
}

void dispatchCmd(int argc, char *argv[])
{
    if(argc < 2)
        throw ArgError("no command");

    list<const char*> args{argv+1, argv+argc};

    //get options
    SrtOpt opt = readOutOpt(args);

    //get cmd
    const char *cmd = args.front();
    args.pop_front();

    if(strcmp(cmd, "merge") == 0)
        return dispatchMerge(opt, args);
    else if(strcmp(cmd, "offset") == 0)
        return dispatchOffset(opt, args);
    else if(strcmp(cmd, "sync") == 0)
        return dispatchSync(opt, args);
    else
        throw ArgError(string("no command '") + cmd + "'");
}

int main(int argc, char *argv[])
{
	try{
        using namespace std::chrono;
        auto start = high_resolution_clock::now();

        dispatchCmd(argc, argv);

        auto stop = high_resolution_clock::now();
        cerr << "done in " << duration<double>(stop-start).count() << " seconds."<< endl;
        
	}
    catch(ArgError &ex){
        cerr << "arguments error: " << ex.what() << endl;
        cerr << getHelp() << endl;
    }
	catch(exception &ex){
		cerr << "system error: " << ex.what() << endl;
	}
	catch(...){
		cerr << "system error";
	}

	return 0;
}

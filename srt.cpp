//#include <UnitTest++.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <list>
#include <utility>
#include <locale>
#include <iomanip>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <cctype>
//#include <regex>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <tttoolbox.h>

using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::ifstream;
using std::ostream;
using std::istream;
using std::stringstream;
using std::string;
using std::vector;
using std::set;
using std::list;
using std::pair;
using std::make_pair;
using std::exception;
using std::runtime_error;
using std::locale;
using std::streamsize;
using std::setw;
using std::fixed;
//using std::regex;
using boost::regex;
//using std::smatch;
using boost::smatch;
using boost::posix_time::ptime;
using boost::posix_time::time_period;
using boost::posix_time::time_duration;
using boost::posix_time::time_from_string;
using boost::posix_time::duration_from_string;
using boost::posix_time::time_facet;
using TTToolbox::atoi;
using TTToolbox::locale_guard;
using TTToolbox::fgetlines;
using TTToolbox::GetLineTypes;
using TTToolbox::strBeginWith;
using TTToolbox::strEndWith;
//using TTToolbox::operator<<;

struct SrtOpt{
	bool is_condense = false;
    string bomb;
    string newline;
};

class Srt{
public:
    struct Item{
        Item(int n, const time_period &p, const string &s)
            :sn(n), period(p), text(s) {}
        void print(ostream &os) const;
        int sn;
        time_period period;
        string text;
    };
	//static const char* ENDL;
	static const char* EPHOC;
public:
	//read from file
	Srt(const string &fpath, const SrtOpt &opt);

	//merge srt
	Srt& operator+=(const Srt &rhs);

    //index, srt item No. (not index of items_)
    Item& operator[](int i);
    const Item& operator[](int i) const;

    void offset(const time_duration &t);
    void scale(double v);

	//output
	void print(ostream &os) const;
private:
    static ptime scaleTime(const ptime &t, double scale);
	static string getFmtTime(stringstream &ss, const ptime &t); //@@! bad method
    void initNewline(GetLineTypes line_opt);
    void extractBomb(vector<string> &lines);
    vector<string>::const_iterator getItemTail(const vector<string> &lines, vector<string>::const_iterator head);
    void addItem(vector<string>::const_iterator begin, vector<string>::const_iterator end, set<int> *sn_read = 0);
    const Item& getItem(int sn) const;
	void appendItemText(string &text, const string &s);
private:
	SrtOpt opt_;
	vector<Item> items_;
};

//const char* Srt::ENDL = "\r\n";
const char* Srt::EPHOC = "1970-01-01 ";  //the time is not important, just for create ptime

//Ctor ================================
Srt::Srt(const string &fpath, const SrtOpt &opt)
	:opt_(opt)
{
    //open file
	ifstream file(fpath);
	if(!file)
		throw runtime_error("no file '" + fpath + "'");

    //read lines
    GetLineTypes ret_type = GetLineTypes::None;
	vector<string> lines = fgetlines(file, &ret_type);

    //opt
    initNewline(ret_type);
    extractBomb(lines);

    //items
    set<int> sn_read;
    for(auto head = lines.cbegin(), tail = lines.cend();
            (tail = getItemTail(lines, head)) > head;
            head = tail)
    {
        addItem(head, tail, &sn_read);
    }
}

//member functions ================================
void Srt::extractBomb(vector<string> &lines)
{
    if(lines.empty())
        return;

    string &s = lines[0];
    size_t pos = 0;
    for(; pos < s.length(); ++pos)
        if(isascii(s[pos]))
            break;
    opt_.bomb = s.substr(0, pos);
    s.erase(0, pos);
}

void Srt::initNewline(GetLineTypes line_type)
{
    if(opt_.newline.empty())
        opt_.newline =
            (line_type == GetLineTypes::Win)? "\r\n":
            (line_type == GetLineTypes::Mac)? "\r":
            (line_type == GetLineTypes::Unix)? "\n":
            "\n"; //default
}

vector<string>::const_iterator Srt::getItemTail(const vector<string> &lines, vector<string>::const_iterator head)
{
	static regex is_period{R"((\d\d:\d\d:\d\d,\d*)\s*-->\s*(\d\d:\d\d:\d\d,\d*))"}; //00:03:20,234 --> 00:03:21,443
    bool has_period = false;

    for(auto it = head; it != lines.cend(); ++it)
    {
        //normal split
        if(it->empty()){
            return it +1;
        }
        //error-proof
        else if(regex_match(*it, is_period)){
            if(has_period){
                return it -1;
            }
            has_period = true;
        }
    }

    return lines.cend();
}

void Srt::appendItemText(string &text, const string &s)
{
	//tags
	static vector<pair<string,string>> tags = {
		{"<i>", "</i>"},
		{"<b>", "</b>"},
		{"<u>", "</u>"}
	};

	//check
	if(s.empty())
		return;
	if(text.empty()){
		text = s;
		return;
	}

    //normal
    if(!opt_.is_condense){
		text += opt_.newline;
        text += s;
    }
    //combine multi lines
    else{
        auto sbegin = s.cbegin();
        auto send = s.cend();

		//remove tag
		for(const auto &tag: tags){
			if(strEndWith(text, tag.second) && strBeginWith(s, tag.first)){
				text.erase(text.length() -tag.second.length());
				sbegin += tag.first.length();
				break;
			}
		}

		//remove hyper, if(alpha, hyper, alpha)
		if(text.size() >= 2 && text.back() == '-' && isalpha(text[text.size()-2]) && isalpha(s.front()))
			text.pop_back();
		//normal
		else
			text += ' ';

        //append
	    text.append(sbegin, send);
	}
}

void Srt::addItem(vector<string>::const_iterator begin, vector<string>::const_iterator end, set<int> *sn_read)
{
    if(end - begin < 2)
        return;

    auto it = begin;

    //sn
    int sn = atoi(*it);
    ++it;

    //sn warn
    if(sn <= 0)
        cerr << "[warning] not positive sn '" << sn << "'" << endl;
    if(sn_read && !sn_read->insert(sn).second)  //not insert ok
        cerr << "[warning] duplicated sn '" << sn << "'" << endl;

    //period
	smatch matches;
	regex is_period{R"((\d\d:\d\d:\d\d,\d*)\s*-->\s*(\d\d:\d\d:\d\d,\d*))"}; //00:03:20,234 --> 00:03:21,443
    if(!regex_search(*it, matches, is_period) || matches.size() != 3){
        string msg = string("bad period format [") + *it + "]";
		throw runtime_error(msg);
    }
    time_period period(
            time_from_string(EPHOC + matches[1]), 
            time_from_string(EPHOC + matches[2]));
    ++it;

    //text
    string text;
    for(; it < end; ++it)
        appendItemText(text, *it);

    //create
    items_.emplace_back(sn, period, text);
}

inline
bool intersects_most(const time_period &p1, const time_period &p2)
{
    if(p1.intersects(p2)){
        auto intersect = p1.intersection(p2);
        if(intersect.length() >= (p1.length() /2) &&
           intersect.length() >= (p2.length() /2))
            return true;
    }
    return false;
}

Srt& Srt::operator+=(const Srt &rhs)
{
	vector<Item> items;
    int sn = 0;

	for(auto i = items_.cbegin(), i_end = items_.cend(),
             j = rhs.items_.cbegin(), j_end = rhs.items_.cend();
        i != i_end && j != j_end;)
	{
		//is intersect at most period
		if(i != i_end && j != j_end && intersects_most(i->period, j->period)){
			items.emplace_back(
                    ++sn,
					i->period.merge(j->period),
					i->text + opt_.newline + j->text);
			++i;
			++j;
		}
		//before: insert lhs's
		else if(j == j_end || i->period.begin() < j->period.begin()){
			items.push_back(*i);
			++i;
		}
		//after: insert rhs's
		else{
			items.push_back(*j);
			++j;
		}
	}

	//swap impl
	items_ = move(items);

	return *this;
}


const Srt::Item& Srt::getItem(int sn) const
{
    auto cond = [=](const Item &item){return item.sn == sn;};

    //guess quick begin
    size_t idx = sn -1;
    if(idx >= items_.size()) idx = 0;
    auto quick_begin = items_.cbegin() + idx;

    //find
    auto it = std::find_if(quick_begin, items_.cend(), cond);
    if(it == items_.cend())
        it = std::find_if(items_.cbegin(), quick_begin, cond);

    if(it == items_.cend()){
        stringstream msg;
        msg << "the SN '" << sn << "' of srt does not exist";
        throw runtime_error(msg.str());
    }

    return *it;
}

Srt::Item& Srt::operator[](int idx)
{
    return const_cast<Item&>(getItem(idx));
    //return getItem(idx);
}

const Srt::Item& Srt::operator[](int idx) const
{
    return getItem(idx);
}

//@@!
//beacause I dont know hown to print ptime, with fragment precison=3, 
//I do it by getting the string first, then cutting extra preciosn.
//This is a bad method, should be removed future!
string Srt::getFmtTime(stringstream &ss, const ptime &t)
{
    //get string
	ss.str("");
	ss << (t + boost::posix_time::microseconds(500));   //round off
	string fmt_t = ss.str();

	size_t idx = fmt_t.find(',');
	if(idx != string::npos && (fmt_t.length() - ++idx) > 3)
		fmt_t.erase(idx +3);   //fragment precision = 3;

	return fmt_t;
}

void Srt::print(ostream &os) const
{
	//
	stringstream ss;
	locale_guard sloc{ss,
		locale{ss.getloc(), new time_facet("%H:%M:%S,%f")}};
	/*/
	locale_guard loc{os,
		locale{os.getloc(), new time_facet("%H:%M:%S,%fff")}};
	//*/

    os << opt_.bomb;

	int sn = 0;
	for(const Item &i: items_){
		os << ++sn << opt_.newline;
		//os << fixed << i.first.begin() << " --> "  << i.first.end() << opt_.newline;
		os << getFmtTime(ss, i.period.begin()) << " --> "  << getFmtTime(ss, i.period.end()) << opt_.newline;
		os << i.text << opt_.newline;
		os << opt_.newline;
	}
}

void Srt::offset(const time_duration &t)
{
    for(Item &item: items_)
        item.period.shift(t);
}

ptime Srt::scaleTime(const ptime &t, double scale)
{
    static ptime ephoc = time_from_string(Srt::EPHOC + string("00:00:00"));

    long ms = (t - ephoc).total_milliseconds() * scale;
    return ephoc + boost::posix_time::milliseconds(ms);
    
}

void Srt::scale(double v)
{
    for(Item &item: items_){
        ptime begin = scaleTime(item.period.begin(), v);
        ptime end = scaleTime(item.period.end(), v);
        item.period = time_period{begin, end};
    }

}
//SrtItem ================================
void Srt::Item::print(ostream &os) const
{
    os << '{' << sn << ", " << period << ", " << text << '}';
}


//global functions ================================
Srt operator+(const Srt &s1, const Srt &s2) 
{
	Srt s{s1};
	s += s2;
	return s;
}

ostream &operator<<(ostream &os, const Srt &srt)
{
	srt.print(os);
	return os;
}

ostream &operator<<(ostream &os, const Srt::Item &item)
{
	item.print(os);
	return os;
}

//=================================================================
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

    //create 1st
    Srt s{*arg, opt};
    ++arg;

    //merge the rest
    for(; arg != arg_end; ++arg)
        s += Srt{*arg, opt};

    cout << s;
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
ptime getSpecTime(const char *str)
{
    static regex is_time{getTimeRegexStr()};

    if(!regex_match(str, is_time))
        throw ArgError("invalid item time");

    return time_from_string(string(Srt::EPHOC) + str);
}

time_duration getOffsetTime(const char *str)
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
		getOffsetTime(args.front()):
		getSpecTime(args.back()) - srt[getSyncSn(args.front())].period.begin();

	srt.offset(offset);
	
	//output
	cout << srt;
}

inline
bool isOffsetTime(const char *str)
{
    return (*str == '+') || (*str == '-');
}

void dispatchSync(const SrtOpt &opt, const list<const char*> &args)
{
    //scale & offset
    if(args.size() == 5){
        //srt
        Srt srt{args.back(), opt};

        auto arg = args.begin();

        //get 1st sn,time pair
        int sn1 = getSyncSn(*arg++);
        ptime time1 = isOffsetTime(*arg)?
            srt[sn1].period.begin() + getOffsetTime(*arg++) :
            getSpecTime(*arg++);

        //get 2nd sn,time pair
        int sn2 = getSyncSn(*arg++);
        ptime time2 = isOffsetTime(*arg)?
            srt[sn2].period.begin() + getOffsetTime(*arg++) :
            getSpecTime(*arg++);

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

        cout << srt;
    }
    else
        throw ArgError("bad sync arguments");
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

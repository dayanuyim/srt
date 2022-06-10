#include "srt.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <utility>
#include <locale>
//#include <iomanip>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <cctype>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <tttoolbox.h>

using std::cerr;
using std::endl;
using std::ifstream;
using std::ostream;
using std::ostringstream;
using std::string;
using std::vector;
using std::set;
using std::pair;
using std::runtime_error;
using std::locale;
//using std::streamsize;
//using std::setw;
//using std::fixed;
using boost::regex;
using boost::smatch;
using boost::posix_time::ptime;
using boost::posix_time::time_period;
using boost::posix_time::time_duration;
using boost::posix_time::time_from_string;
using boost::posix_time::time_facet;
using TTToolbox::atoi;
using TTToolbox::locale_guard;
using TTToolbox::fgetlines;
using TTToolbox::GetLineTypes;
using TTToolbox::strBeginWith;
using TTToolbox::strEndWith;

//static ================================
ptime Srt::toTime(const string &t_str)
{
    return time_from_string("1970-01-01 " + t_str);
}

ptime Srt::epoch()
{
    return toTime("00:00:00");
}

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
    for(auto head = lines.cbegin(), tail = lines.cend();
            (tail = getItemTail(lines, head)) > head;
            head = tail)
    {
        if(opt_.is_verbose) cerr << "[info] Block: Line "<< lineNum(lines, head) << " to " << lineNum(lines, tail) << endl;
        readItemBlock(head, tail);
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

void Srt::readItemBlock(vector<string>::const_iterator begin, vector<string>::const_iterator end)
{
    if(end - begin < 2)
        return;

    auto it = begin;

    //sn
    int sn = atoi(*it);
    ++it;

    //sn warn
    if(sn <= 0)
        cerr << "[warning] invalid sn '" << sn << "' from content '" << *it << "'" << endl;
    if(!sn_read_.insert(sn).second)  //not insert ok
        cerr << "[warning] duplicated sn '" << *it << "'" << endl;

    //period
	smatch matches;
	regex is_period{R"((\d\d:\d\d:\d\d,\d*)\s*-->\s*(\d\d:\d\d:\d\d,\d*))"}; //00:03:20,234 --> 00:03:21,443
    if(!regex_search(*it, matches, is_period) || matches.size() != 3){
        ostringstream msg;
        msg << "bad period format: sn '" << sn << "' has [" << *it << "]";
		throw runtime_error(msg.str());
    }
    time_period period(toTime(matches[1]), toTime(matches[2]));
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


void Srt::addItem(vector<Item> &items, const time_period &p, const string &txt, bool allow_crop)
{
    int sn = items.size() + 1;
    if(sn == 1)
        items.emplace_back(sn, p, txt);
    else{
        ptime curr = items.back().period.end();
        if(curr <= p.begin())
            items.emplace_back(sn, p, txt);
        else if (curr < p.end() && allow_crop)
            items.emplace_back(sn, time_period(curr, p.end()), txt);  //crop begin time
        else{
            //skip if curr >= p.end()
        }
    }
}

Srt& Srt::operator+=(const Srt &rhs)
{
	vector<Item> items;

	for(auto i = items_.cbegin(), i_end = items_.cend(),
             j = rhs.items_.cbegin(), j_end = rhs.items_.cend();
        i != i_end && j != j_end;)
	{
        if(i->period.intersects(j->period)){
            time_period inter = i->period.intersection(j->period);
            addItem(items, 
					inter,
					i->text + opt_.newline + j->text);
            //iterate
            if(i->period.end() == inter.end()) ++i;
            if(j->period.end() == inter.end()) ++j;
        }
        else{
            auto &earlier = (i->period < j->period)? i: j;
            addItem(items, earlier->period, earlier->text, false);  // false: not allow crop.
                                                                    // if crop is needed, the item will be dropped
            ++earlier;
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
        ostringstream msg;
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

ostringstream _getFmtTimeStream()
{
	ostringstream ss;
    std::locale loc{ss.getloc(), new time_facet("%H:%M:%S,%f")};
    ss.imbue(loc);
    return ss;
}

//@@!
//beacause I dont know hown to print ptime, with fragment precison=3, 
//I do it by getting the string first, then cutting extra preciosn.
//Hope to remove the function late!
string Srt::fmtTime(const ptime &t)
{
	static ostringstream ss = _getFmtTimeStream();
    static const int ndigit = 3;

    //get string
	ss.str("");
	ss << (t + boost::posix_time::microseconds(500));   //round off
	string fmt_t = ss.str();

    //strip microsec part
	size_t idx = fmt_t.find(',');
	if(idx != string::npos){
        idx += 1 + ndigit;      //idx of millisec end
        if(idx < fmt_t.length())
		    fmt_t.erase(idx);
    }
	return fmt_t;
}

void Srt::print(ostream &os) const
{
    os << opt_.bomb;

	int sn = 0;
	for(const Item &i: items_){
		os << ++sn << opt_.newline;
		//os << fixed << i.first.begin() << " --> "  << i.first.end() << opt_.newline;
		os << fmtTime(i.period.begin()) << " --> "  << fmtTime(i.period.end()) << opt_.newline;
		os << i.text << opt_.newline;
		os << opt_.newline;
	}
}

Srt& Srt::offset(const time_duration &t)
{
    for(Item &item: items_)
        item.period.shift(t);
    return *this;
}

ptime Srt::scaleTime(const ptime &t, double scale)
{
    ptime base = Srt::epoch();
    long ms = (t - base).total_milliseconds() * scale;
    return base + boost::posix_time::milliseconds(ms);
    
}

Srt& Srt::scale(double v)
{
    for(Item &item: items_){
        ptime begin = scaleTime(item.period.begin(), v);
        ptime end = scaleTime(item.period.end(), v);
        item.period = time_period{begin, end};
    }
    return *this;
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

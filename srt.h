#ifndef __SRT_H
#define __SRT_H

#include <iostream>
#include <string>
#include <vector>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <tttoolbox.h>

using std::ostream;
using std::stringstream;
using std::string;
using std::vector;
using std::set;
using boost::posix_time::ptime;
using boost::posix_time::time_period;
using boost::posix_time::time_duration;
using TTToolbox::GetLineTypes;

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
    static ptime toTime(const string &t);
    static ptime epoch();
public:
	//read from file
	Srt(const string &fpath, const SrtOpt &opt);

	//merge srt
	Srt& operator+=(const Srt &rhs);

    //index, srt item No. (not index of items_)
    Item& operator[](int i);
    const Item& operator[](int i) const;

    Srt& offset(const time_duration &t);
    Srt& scale(double v);
    template<class T> Srt& filter(T cond);

	//output
	void print(ostream &os) const;
private:
    static ptime scaleTime(const ptime &t, double scale);
	static string getFmtTime(stringstream &ss, const ptime &t); //@@! bad method
    static void addItem(vector<Item> &items, const time_period &p, const string &txt, bool allow_crop=true);
    void initNewline(GetLineTypes line_opt);
    void extractBomb(vector<string> &lines);
    vector<string>::const_iterator getItemTail(const vector<string> &lines, vector<string>::const_iterator head);
    void readItemBlock(vector<string>::const_iterator begin, vector<string>::const_iterator end);
    const Item& getItem(int sn) const;
	void appendItemText(string &text, const string &s);
private:
	SrtOpt opt_;
	vector<Item> items_;
	set<int> sn_read_;   //tracking already read sn
};

template<class T>
Srt& Srt::filter(T cond)
{
    for(auto it = items_.begin(); it != items_.end(); it++){
        if(!cond(const_cast<const Item&>(*it)))
            items_.erase(it);
    }

    return *this;
}

Srt operator+(const Srt &s1, const Srt &s2);
ostream &operator<<(ostream &os, const Srt &srt);
ostream &operator<<(ostream &os, const Srt::Item &item);


#endif

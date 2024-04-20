#include <boost/regex.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <ncurses.h>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace std::chrono;

// Source: https://gist.github.com/meritozh/f0351894a2a4aa92871746bf45879157
std::string exec(const char* cmd) {
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe)
		throw runtime_error("popen() failed!");
	char buffer[128];
	std::string result = "";
	while (!feof(pipe.get())) {
		if (fgets(buffer, 128, pipe.get()) != NULL)
			result += buffer;
	}
	return result;
}

struct container_element {
	string stream;
	string lang;
	string type;
	string ext;
	string name;
	string fname;

	bool selected;
	int attachment_count;
};

static string fname;
static int attachments_count = 0;
bool op_i, op_n;

static int usize(string s) {
	int len = 0, i = 0;
	while (s[i])
		len += (s[i++] & 0xc0) != 0x80;
	return len;
}

static std::string fixed_str(string str, int width) {
	int ssize = usize(str);
	if (ssize > width) {
		if (width > 3)
			str = str.substr(0, width - 1) + "…";
		else
			str = str.substr(0, width);
	} else
		str = string(width - ssize, ' ') + str;

	return str;
}

static struct container_list {
	vector<container_element> options = {};
	int abs_start = 0;
	int rel_cursor = 0;
	int window_height;

	void show(int n) {
		window_height = n;
		for (int pos = 0, abs_pos = abs_start;
			 pos < window_height && abs_pos < options.size();
			 ++pos, ++abs_pos) {
			if (pos == rel_cursor)
				attron(A_REVERSE);
			auto& elem = options[abs_pos];
			mvprintw(pos + 1, 1, "%s| %s| %s| %s| %s| %s",
					 fixed_str(elem.selected ? "* " : " ", 2).c_str(),
					 fixed_str(elem.type, 15).c_str(),
					 fixed_str(elem.name, 30).c_str(),
					 fixed_str(elem.ext, 5).c_str(),
					 fixed_str(elem.lang, 10).c_str(),
					 fixed_str(elem.fname, 20).c_str());
			attroff(A_REVERSE);
		}
	}

	void fix_rel_cursor() { rel_cursor = min(rel_cursor, window_height - 1); }

	void go_up() {
		fix_rel_cursor();
		if (rel_cursor == 0) {
			if (abs_start > 0)
				--abs_start;
		} else {
			--rel_cursor;
		}
	}

	void go_down() {
		fix_rel_cursor();
		if (rel_cursor == window_height - 1) {
			if (abs_start < options.size() - 1)
				++abs_start;
		} else {
			++rel_cursor;
		}
	}

	void select() { options[abs_start + rel_cursor].selected ^= true; }

	void select_all() {
		for (auto& elem : options) {
			elem.selected = true;
		}
	}

	void select_none() {
		for (auto& elem : options) {
			elem.selected = false;
		}
	}
} menu;

static int extract_info(const string& s) {
	const char* rstr =
		R"(^\s*Stream\s*#([0-9:]+)(\(.*?\))?:\s*(\w+):\s*(\S*).*?(?:(?:\n(?! {0,2}\S)[^\n]*)*title\s*:\s*(.*?))?(?:(?:\n(?! {0,2}\S)[^\n]*)*filename\s*:\s*(.*?))?$)";
	const boost::regex r(rstr);

	steady_clock::time_point begin = steady_clock::now();

	const vector<boost::smatch> matches = {
		boost::sregex_iterator{cbegin(s), cend(s), r},
		boost::sregex_iterator{}};

	menu.options.reserve(matches.size());
	for (auto& m : matches) {
        if (m[3] != "Subtitle") {
            continue;
        }
		menu.options.push_back(
			container_element{m[1], m[2], m[3], m[4], m[5], m[6],
							  m[3] != "Video" && m[3] != "Audio",
							  m[3] == "Attachment" ? ++attachments_count : 0});
	}

	return duration_cast<microseconds>(steady_clock::now() - begin).count();
}

static int run_interactive() {
	setlocale(LC_ALL, "");
	initscr();
	raw();
	nonl();
	keypad(stdscr, true);
	noecho();

	int cancelled = 0;
	while (true) {
		int ch;
		menu.show(getmaxy(stdscr));

		refresh();
		ch = getch();

		if (ch == 259) {
			menu.go_up();
			continue;
		}

		if (ch == 258) {
			menu.go_down();
			continue;
		}

		if (ch == ' ') {
			menu.select();
			continue;
		}

		if (ch == 'a') {
			menu.select_all();
			continue;
		}

		if (ch == 'n') {
			menu.select_none();
			continue;
		}

		if (ch == 13) {
			break;
		}

		if (ch == 'q') {
			cancelled = 1;
			break;
		}
	}

	endwin();

	return cancelled;
}

static string replace_ext(string f, string ext, string add = "") {
	const boost::regex r(R"(^(.*)\.(.*)$)");
	boost::smatch m;
	if (boost::regex_match(f, m, r)) {
		return m[1] + add + "." + ext;
	} else {
		return f + add + "." + ext;
	}
}

static void xsystem(string cmd) {
	if (op_n) {
		cout << cmd << endl;
	} else {
		system(cmd.c_str());
	}
}

static void extract_data() {
	string outdir = fname + " extracted";
	string cmd;
	cmd = "mkdir \"" + outdir + "\"";
	xsystem(cmd);

	set<string> unique_ext = {};
	for (const auto& elem : menu.options) {
		if (!elem.selected)
			continue;

		string ext = elem.ext;

		if (elem.type == "Video") {
			ext = "mp4";
		} else if (elem.type == "Audio") {
			ext = "mp3";
		}

		string elem_fname;
		if (elem.fname.size() != 0) {
			elem_fname = elem.fname;
		} else if (elem.name.size() != 0) {
			elem_fname = replace_ext(fname, ext, " " + elem.name);
		} else if (elem.lang.size() != 0) {
			elem_fname = replace_ext(fname, ext, " " + elem.lang);
		} else if (!unique_ext.count(ext)) {
			elem_fname = replace_ext(fname, ext);
			unique_ext.insert(ext);
		} else {
			elem_fname = replace_ext(fname, ext, " " + elem.stream);
		}

		cmd = "ffmpeg -i \"" + fname + "\" -map " + elem.stream +
			  " -c copy \"" + outdir + "/" + elem_fname + "\"";
		xsystem(cmd);
	}
}

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--interactive")) {
				op_i = true;
				continue;
			}
			if (!strcmp(argv[i], "-n")) {
				op_n = true;
				continue;
			}
			throw std::invalid_argument("unknown option: " + string(argv[i]));
		} else {
			fname = argv[i];
		}
	}
	if (fname.size() == 0) {
		throw std::invalid_argument("specify container file name!");
	}

	if (access(fname.c_str(), F_OK)) {
		throw std::runtime_error("file does not exist!");
	}

	string cmd = "ffprobe " + string(fname) + " 2>&1";
	string cmd_out = exec(cmd.c_str());
	int regex_time = extract_info(cmd_out);

	sort(menu.options.begin(), menu.options.end(),
		 [](const container_element& x, const container_element& y) {
			 return x.type < y.type;
		 });

	int cancelled = false;
	if (op_i) {
		cancelled = run_interactive();
	}

	if (!cancelled) {
		cout << "[TIME]: ffprobe regex time = " << regex_time << " μs" << endl;
		extract_data();
		return 0;
	} else {
		cout << "Operation cancelled" << endl;
		return -1;
	}
}

//
// Created by yuwenyong on 17-4-21.
//

#include "tinycore/utilities/cookie.h"
#include "tinycore/utilities/string.h"


std::string CookieUtil::quote(const std::string &s, const std::vector<char> &legalChars,
                              const std::array<char, 256> &idmap) {
    if (String::translate(s, idmap, legalChars).empty()) {
        return s;
    } else {
        StringVector res;
        res.emplace_back("\"");
        decltype(translator.begin()) iter;
        for (auto c: s) {
            iter = translator.find(c);
            if (iter != translator.end()) {
                res.emplace_back(iter->second);
            } else {
                res.emplace_back(1, c);
            }
        }
        res.emplace_back("\"");
        return boost::join(res, "");
    }
}

std::string CookieUtil::unquote(const std::string &s) {
    if (s.length() < 2) {
        return s;
    }
    if (s.front() != '\"' || s.back() != '\"') {
        return s;
    }
    std::string str = s.substr(1, s.length() - 2);
    StringVector res;
    ssize_t i = 0, j, k, n = str.length();
    boost::cmatch omatch, qmatch;
    while (i < n) {
        boost::regex_search(str.c_str() + i, str.c_str() + n, omatch, octalPatt);
        boost::regex_search(str.c_str() + i, str.c_str() + n, qmatch, quotePatt);
        if (omatch.empty() && qmatch.empty()) {
            res.emplace_back(str.substr(i));
            break;
        }
        j = k = -1;
        if (!omatch.empty()) {
            j = i + omatch.position((boost::cmatch::size_type)0);
        }
        if (!qmatch.empty()) {
            k = i + qmatch.position((boost::cmatch::size_type)0);
        }
        if (!qmatch.empty() && (omatch.empty() || k < j)) {
            res.emplace_back(str.substr(i, k - i));
            res.emplace_back(str[k + 1], 1);
            i = k + 2;
        } else {
            res.emplace_back(str.substr(i, j - i));
            res.emplace_back((char) (std::stoi(str.substr(j + 1, 3), nullptr, 8)), 1);
            i = j + 4;
        }
    }

    return boost::join(res, "");
}

const std::vector<char> CookieUtil::legalChars = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        'A', 'B', 'C', 'D', 'E', 'F', 'G',
        'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~',
};

const std::array<char, 256> CookieUtil::idmap = {{
        '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
        '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
        '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
        '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
        '\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
        '\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
        '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
        '\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',

        '\100', '\101', '\102', '\103', '\104', '\105', '\106', '\107',
        '\110', '\111', '\112', '\113', '\114', '\115', '\116', '\117',
        '\120', '\121', '\122', '\123', '\124', '\125', '\126', '\127',
        '\130', '\131', '\132', '\133', '\134', '\135', '\136', '\137',
        '\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
        '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
        '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
        '\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',

        '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
        '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
        '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
        '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
        '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
        '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
        '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
        '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',

        '\300', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
        '\310', '\311', '\312', '\313', '\314', '\315', '\316', '\317',
        '\320', '\321', '\322', '\323', '\324', '\325', '\326', '\327',
        '\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',
        '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
        '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
        '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
        '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
}};

const std::map<char, std::string> CookieUtil::translator = {
        {'\000', "\\000"}, {'\001', "\\001"}, {'\002', "\\002"},
        {'\003', "\\003"}, {'\004', "\\004"}, {'\005', "\\005"},
        {'\006', "\\006"}, {'\007', "\\007"}, {'\010', "\\010"},
        {'\011', "\\011"}, {'\012', "\\012"}, {'\013', "\\013"},
        {'\014', "\\014"}, {'\015', "\\015"}, {'\016', "\\016"},
        {'\017', "\\017"}, {'\020', "\\020"}, {'\021', "\\021"},
        {'\022', "\\022"}, {'\023', "\\023"}, {'\024', "\\024"},
        {'\025', "\\025"}, {'\026', "\\026"}, {'\027', "\\027"},
        {'\030', "\\030"}, {'\031', "\\031"}, {'\032', "\\032"},
        {'\033', "\\033"}, {'\034', "\\034"}, {'\035', "\\035"},
        {'\036', "\\036"}, {'\037', "\\037"},

// Because of the way browsers really handle cookies (as opposed
// to what the RFC says) we also encode , and ;

        {',', "\\054"}, {';', "\\073"},
        {'"', "\\\""}, {'\\', "\\\\"},

        {'\177', "\\177"}, {'\200', "\\200"}, {'\201', "\\201"},
        {'\202', "\\202"}, {'\203', "\\203"}, {'\204', "\\204"},
        {'\205', "\\205"}, {'\206', "\\206"}, {'\207', "\\207"},
        {'\210', "\\210"}, {'\211', "\\211"}, {'\212', "\\212"},
        {'\213', "\\213"}, {'\214', "\\214"}, {'\215', "\\215"},
        {'\216', "\\216"}, {'\217', "\\217"}, {'\220', "\\220"},
        {'\221', "\\221"}, {'\222', "\\222"}, {'\223', "\\223"},
        {'\224', "\\224"}, {'\225', "\\225"}, {'\226', "\\226"},
        {'\227', "\\227"}, {'\230', "\\230"}, {'\231', "\\231"},
        {'\232', "\\232"}, {'\233', "\\233"}, {'\234', "\\234"},
        {'\235', "\\235"}, {'\236', "\\236"}, {'\237', "\\237"},
        {'\240', "\\240"}, {'\241', "\\241"}, {'\242', "\\242"},
        {'\243', "\\243"}, {'\244', "\\244"}, {'\245', "\\245"},
        {'\246', "\\246"}, {'\247', "\\247"}, {'\250', "\\250"},
        {'\251', "\\251"}, {'\252', "\\252"}, {'\253', "\\253"},
        {'\254', "\\254"}, {'\255', "\\255"}, {'\256', "\\256"},
        {'\257', "\\257"}, {'\260', "\\260"}, {'\261', "\\261"},
        {'\262', "\\262"}, {'\263', "\\263"}, {'\264', "\\264"},
        {'\265', "\\265"}, {'\266', "\\266"}, {'\267', "\\267"},
        {'\270', "\\270"}, {'\271', "\\271"}, {'\272', "\\272"},
        {'\273', "\\273"}, {'\274', "\\274"}, {'\275', "\\275"},
        {'\276', "\\276"}, {'\277', "\\277"}, {'\300', "\\300"},
        {'\301', "\\301"}, {'\302', "\\302"}, {'\303', "\\303"},
        {'\304', "\\304"}, {'\305', "\\305"}, {'\306', "\\306"},
        {'\307', "\\307"}, {'\310', "\\310"}, {'\311', "\\311"},
        {'\312', "\\312"}, {'\313', "\\313"}, {'\314', "\\314"},
        {'\315', "\\315"}, {'\316', "\\316"}, {'\317', "\\317"},
        {'\320', "\\320"}, {'\321', "\\321"}, {'\322', "\\322"},
        {'\323', "\\323"}, {'\324', "\\324"}, {'\325', "\\325"},
        {'\326', "\\326"}, {'\327', "\\327"}, {'\330', "\\330"},
        {'\331', "\\331"}, {'\332', "\\332"}, {'\333', "\\333"},
        {'\334', "\\334"}, {'\335', "\\335"}, {'\336', "\\336"},
        {'\337', "\\337"}, {'\340', "\\340"}, {'\341', "\\341"},
        {'\342', "\\342"}, {'\343', "\\343"}, {'\344', "\\344"},
        {'\345', "\\345"}, {'\346', "\\346"}, {'\347', "\\347"},
        {'\350', "\\350"}, {'\351', "\\351"}, {'\352', "\\352"},
        {'\353', "\\353"}, {'\354', "\\354"}, {'\355', "\\355"},
        {'\356', "\\356"}, {'\357', "\\357"}, {'\360', "\\360"},
        {'\361', "\\361"}, {'\362', "\\362"}, {'\363', "\\363"},
        {'\364', "\\364"}, {'\365', "\\365"}, {'\366', "\\366"},
        {'\367', "\\367"}, {'\370', "\\370"}, {'\371', "\\371"},
        {'\372', "\\372"}, {'\373', "\\373"}, {'\374', "\\374"},
        {'\375', "\\375"}, {'\376', "\\376"}, {'\377', "\\377"},
};

const boost::regex CookieUtil::octalPatt(R"(\\[0-3][0-7][0-7])");
const boost::regex CookieUtil::quotePatt(R"([\\].)");
const char *CookieUtil::legalCharsPatt = R"([\w\d!#%&'~_`><@,:/\$\*\+\-\.\^\|\)\(\?\}\{\=])";
const boost::regex CookieUtil::cookiePattern(
        R"((?x))"
        R"((?<key>)"
        R"([\w\d!#%&'~_`><@,:/\$\*\+\-\.\^\|\)\(\?\}\{\=]+?)"
        R"())"
        R"(\s*=\s*)"
        R"((?<val>)"
        R"("(?:[^\\"]|\\.)*")"
        R"(|)"
        R"(\w{3},\s[\s\w\d-]{9,11}\s[\d:]{8}\sGMT)"
        R"(|)"
        R"([\w\d!#%&'~_`><@,:/\$\*\+\-\.\^\|\)\(\?\}\{\=]*)"
        R"())"
        R"(\s*;?)",
        boost::regex::perl
);


Morsel::Morsel() {
    for (auto &kv: reserved) {
        _items[kv.first] = "";
    }
}

void Morsel::setItem(std::string key, std::string value) {
    boost::to_lower(key);
    if (reserved.find(key) == reserved.end()) {
        std::string error;
        error = "Invalid Attribute " + key;
        ThrowException(CookieError, std::move(error));
    }
    _items[std::move(key)] = std::move(value);
}

void Morsel::set(std::string key, std::string val, std::string codedVal, const std::vector<char> &legalChars,
                 const std::array<char, 256> &idmap) {
    if (reserved.find(boost::to_lower_copy(key)) != reserved.end()) {
        std::string error;
        error = "Attempt to set a reserved key: " + key;
        ThrowException(CookieError, std::move(error));
    }
    if (!String::translate(key, idmap, legalChars).empty()) {
        std::string error;
        error = "Illegal key value: " + key;
        ThrowException(CookieError, std::move(error));
    }
    _key = std::move(key);
    _value = std::move(val);
    _codedValue = std::move(codedVal);
}

std::string Morsel::outputString(const StringMap *attrs) const {
    StringVector result;
    result.emplace_back(String::format("%s=%s", _key.c_str(), _codedValue.c_str()));
    if (!attrs) {
        attrs = &reserved;
    }
    for (auto &kv: _items) {
        if (kv.second.empty()) {
            continue;
        }
        if (attrs->find(kv.first) == attrs->end()) {
            continue;
        }
        if (kv.first == "secure" || kv.first == "httponly") {
            result.emplace_back(reserved.at(kv.first));
        } else {
            result.emplace_back(String::format("%s=%s", reserved.at(kv.first).c_str(), kv.second.c_str()));
        }
    }
    return boost::join(result, "; ");
}

const StringMap Morsel::reserved = {
        {"expires",  "expires"},
        {"path",     "Path"},
        {"comment",  "Comment"},
        {"domain",   "Domain"},
        {"max-age",  "Max-Age"},
        {"secure",   "secure"},
        {"httponly", "httponly"},
        {"version",  "Version"},
};


BaseCookie::DecodeResultType BaseCookie::valueDecode(const std::string &val) {
    return std::make_tuple(val, val);
}

BaseCookie::EncodeResultType BaseCookie::valueEncode(const std::string &val) {
    return std::make_tuple(val, val);
}

std::string BaseCookie::output(const StringMap *attrs, const std::string &header, const std::string &sep) const {
    StringVector result;
    for (auto &kv: _items) {
        result.emplace_back(kv.second.output(attrs, header));
    }
    return boost::join(result, sep);
}

void BaseCookie::parseString(const std::string &str, const boost::regex &patt) {
    ssize_t i = 0, n = str.length();
    Morsel *m = nullptr;
    boost::cmatch match;
    std::string k, v;
    std::string rval, cval;
    while (i < n) {
        boost::regex_search(str.c_str() + i, str.c_str() + n, match, patt);
        if (match.empty()) {
            break;
        }
        k = match["key"].str();
        v = match["val"].str();
        i = match.position((boost::cmatch::size_type)0) + match.length((int)0);
        if (k[0] == '$') {
            if (m) {
                m->setItem(k.substr(1), std::move(v));
            }
        } else if (Morsel::reserved.find(boost::to_lower_copy(k)) != Morsel::reserved.end()) {
            if (m) {
                m->setItem(std::move(k), CookieUtil::unquote(v));
            }
        } else {
            std::tie(rval, cval) = valueDecode(v);
            set(k, std::move(rval), std::move(cval));
            m = &_items.at(k);
        }
    }
}
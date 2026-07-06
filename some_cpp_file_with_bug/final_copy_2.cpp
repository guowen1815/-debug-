#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <vector>
#include <array>
#include <deque>
#include <optional>
#include <memory>
#include <map>
#include <utility>
#include <unordered_set>
#include <fstream>

using namespace std;
int M,N,R,K,T;

static vector<string> loadExpectedLines(const string &path) {
    ifstream in(path);
    vector<string> lines;
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// Compare each output line against expected output and stop on the first mismatch.
class CompareBuf : public std::streambuf {
public:
    CompareBuf(std::streambuf *dest, vector<string> expected)
        : dest_(dest), expected_(std::move(expected)) {}

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) return dest_->sputc(ch);
        char c = static_cast<char>(ch);
        if (c == '\n') {
            checkLine();
        } else {
            line_.push_back(c);
        }
        return dest_->sputc(c);
    }

    int sync() override { return dest_->pubsync(); }

private:
    void checkLine() {
        size_t lineNo = index_ + 1;
        if (index_ >= expected_.size()) {
            cerr << "[COMPARE] extra line " << lineNo << " : " << line_ << '\n';
            exit(0);
        }
        const string &expected = expected_[index_];
        if (line_ != expected) {
            cerr << "[COMPARE] mismatch at line " << lineNo << '\n';
            cerr << "  expected: " << expected << '\n';
            cerr << "  actual  : " << line_ << '\n';
            exit(0);
        }
        line_.clear();
        ++index_;
    }

    std::streambuf *dest_ = nullptr;
    vector<string> expected_;
    string line_;
    size_t index_ = 0;
};
/*
每个司令部一开始都有M个生命元( 1 <= M <= 10000)
两个司令部之间一共有N个城市( 1 <= N <= 20 )
arrow的攻击力是R
lion每经过一场未能杀死敌人的战斗，忠诚度就降低K。
要求输出从0时0分开始，到时间T为止(包括T) 的所有事件。T以分钟为单位，0 <= T <= 5000
*/

enum WarriorType { DRAGON = 0, NINJA = 1, ICEMAN = 2, LION = 3, WOLF = 4 };
enum WeaponTypr { SWORD = 0, BOMB = 1, ARROW = 2};
map<int,string> Map = {
    {DRAGON,"dragon"},{NINJA,"ninja"},{ICEMAN,"iceman"},{LION,"lion"},{WOLF,"wolf"}
};

class Weapon {
public:
    int id = -1; // 是什么类型的武器

    // sword
    int sword_atk = -1; // 初始攻击力为拥有它的武士的攻击力的20%
    void sword_atk_decreese() { // 经过一次战斗，变钝
        sword_atk = sword_atk * 80 / 100;
        if (sword_atk == 0) {sword_vanish = true; /* 攻击力为0，武士失去sword */ id = -1;}
    }
    bool sword_vanish = false;

    // arrow
    int arrow_atk = R;
    int arrow_cnt = -1; // arrow被使用的次数，使用三次即被耗尽

    // bomb
    // 拥有bomb的武士，在战斗开始前如果判断自己将被杀死（不论主动攻击敌人，或者被敌人主动攻击都可能导致自己被杀死，而且假设武士可以知道敌人的攻击力和生命值），那么就会使用bomb和敌人同归于尽。武士不预测对方是否会使用bomb
    // 武士使用bomb和敌人同归于尽的情况下，不算是一场战斗，双方都不能拿走城市的生命元，也不影响城市的旗帜。
    Weapon(){}
    Weapon(int num,int warrior_atk):id(num) {
        switch (num) {
            case SWORD:{
                sword_atk = warrior_atk / 5;
                arrow_cnt = -1;
                if (sword_atk == 0) id = -1;
                break;
            }
            case BOMB:
                break;
            default:{
                sword_atk = -1;
                arrow_cnt = 0;
            }
        }

    }
    Weapon& operator=(const Weapon& other) {
        if (this == &other) return *this;
        id = other.id;
        sword_atk = other.sword_atk;
        sword_vanish = other.sword_vanish;
        arrow_atk = other.arrow_atk;
        arrow_cnt = other.arrow_cnt;
        if (sword_atk == 0) {
            Weapon tmp; // use default Weapon values instead of calling operator= again
            id = tmp.id;
            sword_atk = tmp.sword_atk;
            sword_vanish = tmp.sword_vanish;
            arrow_atk = tmp.arrow_atk;
            arrow_cnt = tmp.arrow_cnt;
        }
        return *this;
    }
    // 复制构造函数：若 sword_atk == 0 则初始化为默认 Weapon()
    Weapon(const Weapon &other) {
        id = other.id;
        sword_atk = other.sword_atk;
        sword_vanish = other.sword_vanish;
        arrow_atk = other.arrow_atk;
        arrow_cnt = other.arrow_cnt;
        if (sword_atk == 0) {
            *this = Weapon();
        }
    }
    Weapon(int a1,int a2,bool a3,int a4,int a5):id(a1),sword_atk(a2),sword_vanish(a3),arrow_atk(a4),arrow_cnt(a5){}
};

class Warrior {
public:
    string side; // 哪一方的武士
    int type = -1; // 用于判断是什么派生类
    int index;
    int life_value;
    int attack_value;
    double morale = -1;
    bool beaten_by_arrow = false; // 每次战斗结束后更新
    bool dead_because_of_bomb = false;
    bool dragon_attack_first = false; // 每次战斗结束后更新
    int loyality = -1; // 针对lion
    int step = 0; // 针对iceman走路会掉血
    virtual~Warrior(){}
    virtual bool check_weapon(){ return false; }
    virtual vector<Weapon> pass_weapon() {return {};};
    virtual bool have_bomb(){return false;}
    virtual void Bomb(){}
    virtual bool have_sword(){return false;}
    virtual void check_sword(){}
    virtual int get_sword_atk(){return 0;}
    virtual void dragon_func(){}
    virtual void ninja_func(){}
    virtual void iceman_func(){}
    virtual void lion_func(){}
    virtual void wolf_func(){}
};

class Dragon : public Warrior {
public:
    Dragon() { type = DRAGON; }
    //vector<Weapon> pass_weapon() {return {wp};}
    Weapon wp;
    vector<Weapon> pass_weapon() {return {wp};}
    bool check_weapon() {return !(wp.id == -1);}
    bool have_bomb() {return wp.id == BOMB;}
    bool have_sword() {return wp.id == SWORD;}
    void check_sword() {
        if (have_sword()) {
            wp.sword_atk = wp.sword_atk * 4 / 5;
            if (wp.sword_atk == 0) wp = Weapon();
        }
    }
    int get_sword_atk() {
        if (have_sword()) return wp.sword_atk;
        return 0;
    }
    void Bomb() {if (have_bomb()) wp = Weapon();}
    void cheer() {
        //在一次在它主动进攻的战斗结束后，如果还没有战死，而且士气值大于0.8，就会欢呼。dragon每取得一次战斗的胜利(敌人被杀死)，士气就会增加0.2，每经历一次未能获胜的战斗，士气值就会减少0.2。士气增减发生在欢呼之前。
    }
};

class Ninja : public Warrior {
public:
    Ninja() { type = NINJA; }
    Weapon wp1,wp2;
    vector<Weapon> pass_weapon() {return {wp1,wp2};}
    bool have_bomb() {return (wp1.id == BOMB || wp2.id == BOMB);}
    void Bomb() {
        if (have_bomb()) {
            if (wp1.id == BOMB) wp1 = Weapon();
            else wp2 = Weapon();
        }
    }
    bool have_sword() {return (wp1.id == SWORD) || (wp2.id == SWORD);}
    void check_sword() {
        if (wp1.id == SWORD) {
            wp1.sword_atk = wp1.sword_atk * 4 / 5;
            if (wp1.sword_atk == 0) wp1 = Weapon();
        }
        if (wp2.id == SWORD) {
            wp2.sword_atk = wp2.sword_atk * 4 / 5;
            if (wp2.sword_atk == 0) wp2 = Weapon();
        }
    }
    int get_sword_atk() {
        if (have_sword()) {
            if (wp1.id == SWORD) return wp1.sword_atk;
            else return wp2.sword_atk;
        }
        return 0;
    }
    bool check_weapon() {return (!(wp1.id == -1) || !(wp2.id == -1));}
    // ninja 挨打了也从不反击敌人。
};

class Iceman : public Warrior {
public:
    Iceman() { type = ICEMAN; }
    Weapon wp;
    vector<Weapon> pass_weapon() {return {wp};}
    bool have_bomb() {return wp.id == BOMB;}
    void Bomb() {if (have_bomb()) wp = Weapon();}
    bool have_sword() {return wp.id == SWORD;}
    void check_sword() {
        if (have_sword()) {
            wp.sword_atk = wp.sword_atk * 4 / 5;
            if (wp.sword_atk == 0) wp = Weapon();
        }
    }
    int get_sword_atk() {
        if (have_sword()) return wp.sword_atk;
        return 0;
    }
    bool check_weapon() {return !(wp.id == -1);}
    void decrease() {
        // iceman 每前进两步，在第2步完成的时候，生命值会减少9，攻击力会增加20。但是若生命值减9后会小于等于0，则生命值不减9,而是变为1。即iceman不会因走多了而死。
    }
};

class Lion : public Warrior {
public:
    Lion() { type = LION; }
    bool have_bomb(){return false;}
    bool have_sword(){return false;}
    void check_sword(){}
    void Bomb(){}
    int get_sword_atk(){return 0;}
    bool escape = false;
    void func() {
        // 每经过一场未能杀死敌人的战斗，忠诚度就降低K。
        // 忠诚度降至0或0以下，则该lion逃离战场,永远消失。但是已经到达敌人司令部的lion不会逃跑。Lion在己方司令部可能逃跑。
        // lion 若是战死，则其战斗前的生命值就会转移到对手身上。所谓“战斗前”，就是每个小时的40分前的一瞬间
    }
};

class Wolf : public Warrior {
public:
    Wolf() { type = WOLF; }
    Weapon wp1,wp2,wp3;
    vector<Weapon> pass_weapon() {return {wp1,wp2,wp3};}
    bool have_bomb(){return (wp1.id == BOMB || wp2.id == BOMB || wp3.id == BOMB);}
    void Bomb() {
        if (have_bomb()) {
            if (wp1.id == BOMB) wp1 = Weapon();
            else if (wp2.id == BOMB) wp2 = Weapon();
            else wp3 = Weapon();
        }
    }
    bool have_sword(){return (wp1.id == SWORD || wp2.id == SWORD || wp3.id == SWORD);}
    void check_sword(){
        if (wp1.id == SWORD) {
            wp1.sword_atk = wp1.sword_atk * 4 / 5;
            if (wp1.sword_atk == 0) wp1 = Weapon();
        }
        if (wp2.id == SWORD) {
            wp2.sword_atk = wp2.sword_atk * 4 / 5;
            if (wp2.sword_atk == 0) wp2 = Weapon();
        }
        if (wp3.id == SWORD) {
            wp3.sword_atk = wp3.sword_atk * 4 / 5;
            if (wp3.sword_atk == 0) wp3 = Weapon();
        }
    }
    int get_sword_atk() {
        if (have_sword()) {
            if (wp1.id == SWORD) return wp1.sword_atk;
            if (wp2.id == SWORD) return wp2.sword_atk;
            if (wp3.id == SWORD) return wp3.sword_atk;
        }
        return 0;
    }
    bool check_weapon() {return (!(wp1.id == -1) || !(wp2.id == -1) || !(wp3.id == -1));}
    bool have_weapon;
};


// 实现一个class point有两个成员变量，first为第一场战斗的结果（初始化为-2），second为第二场战斗的结果，用于判断是否插旗
// 实现一个vector<point>存储插旗子的情况，1为红旗，0没插旗，-1为蓝旗
// 实现一个vector<Warrior>分别存储红蓝两方武士的位置信息

// 武士使用bomb和敌人同归于尽的情况下，不算是一场战斗，双方都不能拿走城市的生命元，也不影响城市的旗帜
struct city {
    int out_put = 0;
    int first_battle_result = -2; // 战斗结果，-1为蓝方胜，0为平局，1为红方胜
    int second_battle_result = -2;
    int flag = 0;
    unique_ptr<Warrior> red_warrior = nullptr;
    unique_ptr<Warrior> blue_warrior = nullptr;
};
// !!!
// 延迟初始化：city 包含 unique_ptr，不可拷贝，不能用 fill 构造整个 vector
// 请在读取 N 后在 main 中调用 `war_map.resize(N+2);`
// !!!
vector<city> war_map;

const string warriorName[5] = {"dragon", "ninja", "iceman", "lion", "wolf"};
const string weaponName[3] = {"sword", "bomb", "arrow"};
const int redOrder[5] = {ICEMAN, LION, WOLF, NINJA, DRAGON};
const int blueOrder[5] = {LION, DRAGON, NINJA, ICEMAN, WOLF};
const int timeOrder[10] = {0,5,10,20,30,35,38,40,50,55};

/*

*/
int cost[5];
int attack[5];
/*

*/

/*
! 以下类继承自上一次的魔兽作业，慎用！
*/
class Headquarters {
public:
    string side;
    int life = M;
    int nextIndex = 0;
    int totalCreated = 0;
    int typeCount[5] = {0};
    int enemy_cnt = 0; // 任何一方的司令部里若是出现了2个敌人，则认为该司令部已被敌人占领
    bool occupy = false;
    bool called = false; // 是否已经输出过占领司令部的消息
    Headquarters(string str):side(str){}
};
/*
! 以上类继承自上一次的魔兽作业，慎用！
*/

Headquarters redhq("red");
Headquarters bluehq("blue");

string timePrefix(int time_index) {
    ostringstream out; // 构建一个字符串流对象，用于格式化输出
    out << setw(3) << setfill('0') << time_index / 10 << ':' << setw(2) << setfill('0') << timeOrder[time_index % 10] << ' '; // 返回一个格式化后的时间前缀字符串，格式为三位数字，前面补零，并以空格结尾
    // 使用 <iomanip> 中的 setw(3) 将后续输出设为宽度 3，setfill('0') 指定填充字符为 '0'，然后输出 time，最后输出一个空格字符。
    return out.str(); // 将字符串流中的内容转换为字符串并返回
}

/*
输出事件时：

首先按时间顺序输出；

同一时间发生的事件，按发生地点从西向东依次输出. 武士前进的事件, 算是发生在目的地。

在一次战斗中有可能发生上面的 6 至 11 号事件。这些事件都算同时发生，其时间就是战斗开始时间。一次战斗中的这些事件，序号小的应该先输出。

两个武士同时抵达同一城市，则先输出红武士的前进事件，后输出蓝武士的。

显然，13号事件发生之前的一瞬间一定发生了12号事件。输出时，这两件事算同一时间发生，但是应先输出12号事件

虽然任何一方的司令部被占领之后，就不会有任何事情发生了。但和司令部被占领同时发生的事件，全都要输出。
*/

/*
在每个整点，即每个小时的第0分， 双方的司令部中各有一个武士降生。

红方司令部按照 iceman、lion、wolf、ninja、dragon 的顺序制造武士。

蓝方司令部按照 lion、dragon、ninja、iceman、wolf 的顺序制造武士。

制造武士需要生命元。

制造一个初始生命值为 m 的武士，司令部中的生命元就要减少 m 个。

如果司令部中的生命元不足以制造某武士，那么司令部就等待，直到获得足够生命元后的第一个整点，才制造该武士。例如，在2:00，红方司令部本该制造一个 wolf ，如果此时生命元不足，那么就会等待，直到生命元足够后的下一个整点，才制造一个 wolf。
*/
void printBornInfo(const Headquarters &hq, int type, int bornNo) {
    if (type == LION) {
        cout << "Its loyalty is " << hq.life << '\n';
    }
}

void makeWarrior(Headquarters &hq, const int order[5], int time_index) {
    double born_morale = -1.0;
    if (hq.life >= cost[order[hq.nextIndex]]) {
        int type = order[hq.nextIndex];
        hq.life -= cost[type];
        hq.totalCreated++;
        hq.typeCount[type]++;
        int bornNo = hq.totalCreated;
        hq.nextIndex = (hq.nextIndex + 1) % 5;
        unique_ptr<Warrior> warrior;
        switch (type) {
            case DRAGON : {
                auto w = make_unique<Dragon>();
                w->side = hq.side;
                w->index = hq.totalCreated;
                w->life_value = cost[type];
                w->attack_value = attack[type];
                w->wp = Weapon(hq.totalCreated % 3, w->attack_value);
                w->morale = (double)hq.life / w->life_value;
                born_morale = w->morale;
                warrior = move(w);
                if (hq.side == "red") {
                    war_map[0].red_warrior = move(warrior);
                } else {
                    war_map[N + 1].blue_warrior = move(warrior);
                }
                break;
            }
            case NINJA : {
                auto w = make_unique<Ninja>();
                w->side = hq.side;
                w->index = hq.totalCreated;
                w->life_value = cost[type];
                w->attack_value = attack[type];
                w->wp1 = Weapon(hq.totalCreated % 3,w->attack_value);
                w->wp2 = Weapon((hq.totalCreated + 1) % 3,w->attack_value);
                warrior = move(w);
                if (hq.side == "red") {
                    war_map[0].red_warrior = move(warrior);
                } else {
                    war_map[N + 1].blue_warrior = move(warrior);
                }
                break;
            }
            case ICEMAN : {
                auto w = make_unique<Iceman>();
                w->side = hq.side;
                w->index = hq.totalCreated;
                w->life_value = cost[type];
                w->attack_value = attack[type];
                w->wp = Weapon(hq.totalCreated % 3,w->attack_value);
                warrior = move(w);
                if (hq.side == "red") {
                    war_map[0].red_warrior = move(warrior);
                } else {
                    war_map[N + 1].blue_warrior = move(warrior);
                }
                break;
            }
            case LION : {
                auto w = make_unique<Lion>();
                w->side = hq.side;
                w->index = hq.totalCreated;
                w->life_value = cost[type];
                w->attack_value = attack[type];
                w->loyality = hq.life;
                w->escape = w->loyality == 0 ? true : false;
                warrior = move(w);
                if (hq.side == "red") {
                    war_map[0].red_warrior = move(warrior);
                } else {
                    war_map[N + 1].blue_warrior = move(warrior);
                }
                break;
            }
            default : {
                auto w = make_unique<Wolf>();
                w->side = hq.side;
                w->index = hq.totalCreated;
                w->life_value = cost[type];
                w->attack_value = attack[type];
                w->wp1 = Weapon(),w->wp2 = Weapon(),w->wp3 = Weapon();
                w->have_weapon = false;
                warrior = move(w);
                if (hq.side == "red") {
                    war_map[0].red_warrior = move(warrior);
                } else {
                    war_map[N + 1].blue_warrior = move(warrior);
                }
            }
        }
        cout << timePrefix(time_index) << hq.side << ' ' << warriorName[type] << ' ' << bornNo << " born" << '\n';
        printBornInfo(hq, type, bornNo);
        if (type == DRAGON) {
            cout << "Its morale is " << fixed << setprecision(2) << born_morale << '\n';
        }
    }
    else return;
}

void lion_escape(Headquarters &hq, int time_index, int situation) {
    // !!!这里暂时没有处理lion到对方司令部的特殊情况
    city& cell = war_map[situation];
    unique_ptr<Warrior> *warrior = (hq.side == "red") ? &cell.red_warrior : &cell.blue_warrior;

    if (!(*warrior)) {
        return;
    }

    Lion* lion = dynamic_cast<Lion*>((*warrior).get());
    if (!lion) return;

    if (lion->loyality <= 0) {
        if (hq.side == "red") {
            if (situation != N+1) {
                lion->escape = true;
                (*warrior).reset();
                cout << timePrefix(time_index) << hq.side << ' ' << "lion " << lion->index << " ran away\n";
            }
        } 
        else {
            if (situation != 0) {
                lion->escape = true;
                (*warrior).reset();
                cout << timePrefix(time_index) << hq.side << ' ' << "lion " << lion->index << " ran away\n";
            }
        }
    }
}

void Warrior_March(int time_index) {
    struct Snapshot { string side; int type; int index; int life_value; int attack_value; };
    vector<vector<Snapshot>> action_lists(N+2);
    optional<Snapshot> red_reached_blue;
    optional<Snapshot> blue_reached_red;
    // 红方武士前进
    for (int pos=N+1; pos>0; pos--) {
        city& cell = war_map[pos-1];
        if (cell.red_warrior) {
            unique_ptr<Warrior>& warrior = cell.red_warrior;
            warrior->step++;
            int Type = cell.red_warrior->type;
            if (Type == ICEMAN) {
                if (warrior->step % 2 == 0) {
                    if (warrior->life_value <= 9) warrior->life_value = 1;
                    else warrior->life_value -= 9;
                    warrior->attack_value += 20;
                }
            }
            // 在移动之前保存必要的信息到 snapshot
            Snapshot snapR{warrior->side, warrior->type, warrior->index, warrior->life_value, warrior->attack_value};
            war_map[pos].red_warrior = move(war_map[pos-1].red_warrior);
            if (pos >= 1 && pos <= N) {
                action_lists[pos].push_back(snapR);
            }
            if (pos == N+1) {
                bluehq.enemy_cnt ++;
                if (bluehq.enemy_cnt == 2) bluehq.occupy = true;
                red_reached_blue = snapR;
            }
        }
    }
    // 蓝武士前进
    for (int pos=0; pos<=N; pos++) {
        city& cell = war_map[pos+1];
        if (cell.blue_warrior) {
            unique_ptr<Warrior>& warrior = cell.blue_warrior;
            warrior->step++;
            int Type = cell.blue_warrior->type;
            if (Type == ICEMAN) {
                if (warrior->step % 2 == 0) {
                    if (warrior->life_value <= 9) warrior->life_value = 1;
                    else warrior->life_value -= 9;
                    warrior->attack_value += 20;
                }
            }
            Snapshot snapB{warrior->side, warrior->type, warrior->index, warrior->life_value, warrior->attack_value};
            war_map[pos].blue_warrior = move(war_map[pos+1].blue_warrior);
            if (pos >= 1 && pos <= N) {
                action_lists[pos].push_back(snapB);
            }
            if (pos == 0) {
                redhq.enemy_cnt ++;
                if (redhq.enemy_cnt == 2) redhq.occupy = true;
                blue_reached_red = snapB;
            }
        }
    }
/*

*/
    if (blue_reached_red) {
        cout << timePrefix(time_index) << "blue " << warriorName[blue_reached_red->type] << ' ' << blue_reached_red->index
             << " reached red headquarter with " << blue_reached_red->life_value << " elements and force " << blue_reached_red->attack_value << '\n';
        if (redhq.occupy && !redhq.called) {
            cout << timePrefix(time_index) << "red headquarter was taken\n";
            redhq.called = true;
        }
    }

    for (int city=1; city<=N+1; city++) {
        for (const auto& w : action_lists[city]) {
            cout << timePrefix(time_index) << w.side << ' ' << warriorName[w.type] << ' ' << w.index
                 << " marched to city " << city << " with " << w.life_value << " elements and force " << w.attack_value << '\n';
        }
    }

    if (red_reached_blue) {
        cout << timePrefix(time_index) << "red " << warriorName[red_reached_blue->type] << ' ' << red_reached_blue->index
             << " reached blue headquarter with " << red_reached_blue->life_value << " elements and force " << red_reached_blue->attack_value << '\n';
        if (bluehq.occupy && !bluehq.called) {
            cout << timePrefix(time_index) << "blue headquarter was taken\n";
            bluehq.called = true;
        }
    }
/*

*/
}

// 在每个小时的第20分：每个城市产出10个生命元。生命元留在城市，直到被武士取走
void city_output(int time_index) {
    for (int i=1;i<=N;i++) {
        war_map[i].out_put += 10;
    }
}

struct tank1{
    string war="";
    int index=-1;
    int element=-1;
    tank1(){}
};
// 在每个小时的第30分：如果某个城市中只有一个武士，那么该武士取走该城市中的所有生命元，并立即将这些生命元传送到其所属的司令部。
void fetch_element(int time_index) {
    for (int i=1;i<=N;i++) {
        if (!war_map[i].blue_warrior || !war_map[i].red_warrior) {
            if (war_map[i].red_warrior) {
                cout << timePrefix(time_index) << "red " << Map[war_map[i].red_warrior->type] << ' '
                     << war_map[i].red_warrior->index << " earned " << war_map[i].out_put
                     << " elements for his headquarter\n";
                redhq.life += war_map[i].out_put;
                war_map[i].out_put = 0;
            }
            if (war_map[i].blue_warrior) {
                cout << timePrefix(time_index) << "blue " << Map[war_map[i].blue_warrior->type] << ' '
                     << war_map[i].blue_warrior->index << " earned " << war_map[i].out_put
                     << " elements for his headquarter\n";
                bluehq.life += war_map[i].out_put;
                war_map[i].out_put = 0;
            }
        }
    }
}

/*
在每个小时的第35分，拥有arrow的武士放箭，对敌人造成伤害。
放箭事件应算发生在箭发出的城市。注意，放箭不算是战斗，因此放箭的武士不会得到任何好处。
武士在没有敌人的城市被箭射死也不影响其所在城市的旗帜更换情况。
！！！ 不能攻击对方司令部里的敌人
*/
struct tank {
    string war1,war2;
    int id1,id2;
    tank(string str1,int n1,string str2,int n2):war1(str1),id1(n1),war2(str2),id2(n2){}
    void clear() {war1 = "",war2 = "",id1 = -1,id2 = -1;}
};

void throw_arrow(int time_index){
    vector<tank> action_lst;
    for (int i=1;i<=N;i++) {
        if (war_map[i].red_warrior) {
            if (i == N) continue;
            auto& w = war_map[i].red_warrior;
            if (!war_map[i+1].blue_warrior) {
                continue;
            }
            int t = w->type;
            tank temp("",-1,"",-1);
            switch (t) {
                case DRAGON : {
                    auto *dragon = dynamic_cast<Dragon*>(w.get());
                    if (dragon->wp.id == ARROW) {
                        if (dragon->wp.arrow_cnt == 3) dragon->wp = Weapon();
                        else {
                            dragon->wp.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = dragon->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                            if (war_map[i+1].blue_warrior->life_value <= 0) {
                                temp.war2 = "blue " + Map[war_map[i+1].blue_warrior->type],temp.id2 = war_map[i+1].blue_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    break;
// ! 注意在40分钟的时候统一将beaten_by_arrow属性更新为false !!!
                }
                case NINJA : {
                    auto *ninja = dynamic_cast<Ninja*>(w.get());
                    if (ninja->wp1.id == ARROW) {
                        if (ninja->wp1.arrow_cnt == 3) ninja->wp1 = Weapon();
                        else {
                            ninja->wp1.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = ninja->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                            if (war_map[i+1].blue_warrior->life_value <= 0) {
                                temp.war2 = "blue " + Map[war_map[i+1].blue_warrior->type],temp.id2 = war_map[i+1].blue_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    if (ninja->wp2.id == ARROW) {
                        if (ninja->wp2.arrow_cnt == 3) ninja->wp2 = Weapon();
                        else {
                            ninja->wp2.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = ninja->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                            if (war_map[i+1].blue_warrior->life_value <= 0) {
                                temp.war2 = "blue " + Map[war_map[i+1].blue_warrior->type],temp.id2 = war_map[i+1].blue_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    break;
                }
                case ICEMAN : {
                    auto *iceman = dynamic_cast<Iceman*>(w.get());
                    if (iceman->wp.id == ARROW) {
                        if (iceman->wp.arrow_cnt == 3) iceman->wp = Weapon();
                        else {
                            iceman->wp.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = iceman->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                            if (war_map[i+1].blue_warrior->life_value <= 0) {
                                temp.war2 = "blue " + Map[war_map[i+1].blue_warrior->type],temp.id2 = war_map[i+1].blue_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    break;
                }
                case LION : break;
                default : {
                    auto *wolf = dynamic_cast<Wolf*>(w.get());
                    bool c_out = false;
                    if (wolf->wp1.id == ARROW) {
                        if (wolf->wp1.arrow_cnt == 3) wolf->wp1 = Weapon();
                        else {
                            c_out = true;
                            wolf->wp1.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = wolf->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                        }
                    }
                    if (wolf->wp2.id == ARROW) {
                        if (wolf->wp2.arrow_cnt == 3) wolf->wp2 = Weapon();
                        else {
                            c_out = true;
                            wolf->wp2.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = wolf->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                        }
                    }
                    if (wolf->wp3.id == ARROW) {
                        if (wolf->wp3.arrow_cnt == 3) wolf->wp3 = Weapon();
                        else {
                            c_out = true;
                            wolf->wp3.arrow_cnt ++;
                            temp.war1 = "red " + Map[t],temp.id1 = wolf->index;
                            war_map[i+1].blue_warrior->life_value -= R;
                            war_map[i+1].blue_warrior->beaten_by_arrow = true;
                        }
                    }
                    if (c_out && war_map[i+1].blue_warrior->beaten_by_arrow == true) {
                        if (war_map[i+1].blue_warrior->life_value <= 0) {
                            temp.war2 = "blue " + Map[war_map[i+1].blue_warrior->type],temp.id2 = war_map[i+1].blue_warrior->index;
                        }
                        action_lst.push_back(temp);
                        temp.clear();
                    }
                }
            }
        }
        if (war_map[i].blue_warrior) {
            if (i == 1) continue;
            auto& w = war_map[i].blue_warrior;
            if (!war_map[i-1].red_warrior) {
                continue;
            }
            int t = w->type;
            tank temp("",-1,"",-1);
            switch (t) {
                case DRAGON : {
                    auto *dragon = dynamic_cast<Dragon*>(w.get());
                    if (dragon->wp.id == ARROW) {
                        if (dragon->wp.arrow_cnt == 3) dragon->wp = Weapon();
                        else {
                            dragon->wp.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = dragon->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                            if (war_map[i-1].red_warrior->life_value <= 0) {
                                temp.war2 = "red " + Map[war_map[i-1].red_warrior->type],temp.id2 = war_map[i-1].red_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    break;
// ! 注意在40分钟的时候统一将beaten_by_arrow属性更新为false !!!
                }
                case NINJA : {
                    auto *ninja = dynamic_cast<Ninja*>(w.get());
                    if (ninja->wp1.id == ARROW) {
                        if (ninja->wp1.arrow_cnt == 3) ninja->wp1 = Weapon();
                        else {
                            ninja->wp1.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = ninja->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                            if (war_map[i-1].red_warrior->life_value <= 0) {
                                temp.war2 = "red " + Map[war_map[i-1].red_warrior->type],temp.id2 = war_map[i-1].red_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    if (ninja->wp2.id == ARROW) {
                        if (ninja->wp2.arrow_cnt == 3) ninja->wp2 = Weapon();
                        else {
                            ninja->wp2.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = ninja->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                            if (war_map[i-1].red_warrior->life_value <= 0) {
                                temp.war2 = "red " + Map[war_map[i-1].red_warrior->type],temp.id2 = war_map[i-1].red_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    break;
                }
                case ICEMAN : {
                    auto *iceman = dynamic_cast<Iceman*>(w.get());
                    if (iceman->wp.id == ARROW) {
                        if (iceman->wp.arrow_cnt == 3) iceman->wp = Weapon();
                        else {
                            iceman->wp.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = iceman->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                            if (war_map[i-1].red_warrior->life_value <= 0) {
                                temp.war2 = "red " + Map[war_map[i-1].red_warrior->type],temp.id2 = war_map[i-1].red_warrior->index;
                            }
                            action_lst.push_back(temp);
                            temp.clear();
                        }
                    }
                    break;
                }
                case LION : break;
                default : {
                    auto *wolf = dynamic_cast<Wolf*>(w.get());
                    bool c_out = false;
                    if (wolf->wp1.id == ARROW) {
                        if (wolf->wp1.arrow_cnt == 3) wolf->wp1 = Weapon();
                        else {
                            c_out = true;
                            wolf->wp1.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = wolf->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                        }
                    }
                    if (wolf->wp2.id == ARROW) {
                        if (wolf->wp2.arrow_cnt == 3) wolf->wp2 = Weapon();
                        else {
                            c_out = true;
                            wolf->wp2.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = wolf->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                        }
                    }
                    if (wolf->wp3.id == ARROW) {
                        if (wolf->wp3.arrow_cnt == 3) wolf->wp3 = Weapon();
                        else {
                            c_out = true;
                            wolf->wp3.arrow_cnt ++;
                            temp.war1 = "blue " + Map[t],temp.id1 = wolf->index;
                            war_map[i-1].red_warrior->life_value -= R;
                            war_map[i-1].red_warrior->beaten_by_arrow = true;
                        }
                    }
                    if (c_out && war_map[i-1].red_warrior->beaten_by_arrow == true) {
                        if (war_map[i-1].red_warrior->life_value <= 0) {
                            temp.war2 = "red " + Map[war_map[i-1].red_warrior->type],temp.id2 = war_map[i-1].red_warrior->index;
                        }
                        action_lst.push_back(temp);
                        temp.clear();
                    }
                }
            }
        }
    }
    for (const tank& act : action_lst) {
        if (act.id2 == -1) cout<< timePrefix(time_index) << act.war1 << " " << act.id1 << " shot"<<endl;
        else cout<< timePrefix(time_index) << act.war1 << " " << act.id1 << " shot and killed " << act.war2 << " " << act.id2 << endl;
    }
    action_lst.clear();
}

/*
struct tank {
    string war1,war2;
    int id1,id2;
    tank(string str1,int n1,string str2,int n2):war1(str1),id1(n1),war2(str2),id2(n2){}
    void clear() {war1 = "",war2 = "",id1 = -1,id2 = -1;}
};
*/

/*
拥有bomb的武士，在战斗开始前如果判断自己将被杀死
（不论主动攻击敌人，或者被敌人主动攻击都可能导致自己被杀死，而且假设武士可以知道敌人的攻击力和生命值）
那么就会使用bomb和敌人同归于尽。武士不预测对方是否会使用bomb。
*/

/* 原来的代码，逻辑好像和题目有比较大的偏差
vector<tank> action_lst;
    for (int i=1;i<=N;i++) {
        auto &rptr = war_map[i].red_warrior, &bptr = war_map[i].blue_warrior;
        if (rptr && bptr) {
            // 红武士评估
            // 在插红旗的城市，以及编号为奇数的无旗城市，由红武士主动发起进攻。
            // 在插蓝旗的城市，以及编号为偶数的无旗城市，由蓝武士主动发起进攻。
            if ((war_map[i].flag == 1) || (war_map[i].flag == 0 && i % 2 == 1)) {
                // 红武士主动攻击
                int red_atk = rptr->attack_value + rptr->get_sword_atk();
                int blue_ctk = bptr->attack_value / 2 + bptr->get_sword_atk();
                if (bptr->life_value - red_atk > 0 && rptr->life_value - blue_ctk <= 0) {
                    rptr->life_value = -1,bptr->life_value = -1;
                    tank ta("red "+Map[rptr->type],rptr->index,"blue "+Map[bptr->type],bptr->index);
                    if (!rptr->dead_because_of_bomb) action_lst.push_back(ta);
                    rptr->dead_because_of_bomb = true,bptr->dead_because_of_bomb = true;
                    rptr->Bomb();
                }
            }
            if ((war_map[i].flag == -1) || (war_map[i].flag == 0 && i % 2 == 0)) {
                // 蓝武士主动攻击
                int blue_atk = bptr->attack_value + bptr->get_sword_atk();
                if (rptr->life_value - blue_atk <= 0) {
                    rptr->life_value = -1,bptr->life_value = -1;
                    tank ta("red "+Map[rptr->type],rptr->index,"blue "+Map[bptr->type],bptr->index);
                    if (!rptr->dead_because_of_bomb) action_lst.push_back(ta);
                    rptr->dead_because_of_bomb = true,bptr->dead_because_of_bomb = true;
                    rptr->Bomb();
                }
            }
            // 蓝武士评估
            // 这里我理解的是如果红武士丢过了炸弹没必要再丢一遍？
            if ((war_map[i].flag == -1) || (war_map[i].flag == 0 && i % 2 == 0)) {
                // 蓝武士主动攻击
                int blue_atk = bptr->attack_value + bptr->get_sword_atk();
                int red_ctk = rptr->attack_value / 2 + rptr->get_sword_atk();
                if (rptr->life_value - blue_atk > 0 && bptr->life_value - red_ctk <= 0) {
                    bptr->life_value = -1, rptr->life_value = -1;
                    tank ta("blue " + Map[bptr->type],bptr->index,"red " + Map[rptr->type],rptr->index);
                    if (!bptr->dead_because_of_bomb) action_lst.push_back(ta);
                    bptr->dead_because_of_bomb = true, rptr->dead_because_of_bomb = true;
                    bptr->Bomb();
                }
            }
            if ((war_map[i].flag == 1) || (war_map[i].flag == 0 && i % 2 == 1)) {
                // 红武士主动攻击
                int red_atk = rptr->attack_value + rptr->get_sword_atk();
                if (bptr->life_value - red_atk <= 0) {
                    rptr->life_value = -1,bptr->life_value = -1;
                    tank ta("blue " + Map[bptr->type],bptr->index,"red " + Map[rptr->type],rptr->index);
                    if (!bptr->dead_because_of_bomb) action_lst.push_back(ta);
                    bptr->dead_because_of_bomb = true, rptr->dead_because_of_bomb = true;
                    bptr->Bomb();
                }
            }
        }
    }

    for (const tank& act : action_lst) {cout<< timePrefix(time_index) << act.war1 << " " << act.id1 << " used a bomb and killed " << act.war2 << " " << act.id2 << endl;}
    action_lst.clear();
*/

void use_bomb(int time_index) {
//不论主动攻击敌人，或者被敌人主动攻击都可能导致自己被杀死，而且假设武士可以知道敌人的攻击力和生命值
/*
需要修改或同时修改 ownership（用引用，操作后不要再用旧裸指针）：
auto &rptr = war_map[i].red_warrior;
auto &bptr = war_map[i].blue_warrior;
if (!rptr || !bptr) continue;

Warrior *r = rptr.get();
Warrior *b = bptr.get();

// 评估后决定同时自爆（双方同归于尽）
if (r->have_bomb() &&  r 预测会被杀 ) {
    // 输出事件（可使用 r->type, r->index 等）
    rptr.reset();   // 释放红
    bptr.reset();   // 释放蓝
    // 注意：reset 后 r 和 b 都是悬空指针，不能再访问
    continue;
}
*/
    vector<tank> action_lst;
    for (int i=1;i<=N;i++) {
        auto &rptr = war_map[i].red_warrior, &bptr = war_map[i].blue_warrior;
        if (rptr && bptr) {
            int test_red_life = rptr->life_value;
            int test_red_attack = rptr->attack_value;
            string test_red_warrior = rptr->side + Map[rptr->type] + " " + to_string(rptr->index);
            int test_blue_life = bptr->life_value;
            int test_blue_attack = bptr->attack_value;
            string test_blue_warrior = bptr->side + Map[bptr->type] + " " + to_string(bptr->index);

            // 红武士评估
            // 在插红旗的城市，以及编号为奇数的无旗城市，由红武士主动发起进攻。
            // 在插蓝旗的城市，以及编号为偶数的无旗城市，由蓝武士主动发起进攻。
            if ((war_map[i].flag == 1) || (war_map[i].flag == 0 && i % 2 == 1)) {
                // 红武士主动攻击
                int red_atk = rptr->attack_value + rptr->get_sword_atk();
                int blue_ctk = bptr->attack_value / 2 + bptr->get_sword_atk();
                if (bptr->life_value - red_atk > 0 && rptr->life_value - blue_ctk <= 0 && rptr->have_bomb()) {
                    rptr->life_value = -1,bptr->life_value = -1;
                    tank ta("red "+Map[rptr->type],rptr->index,"blue "+Map[bptr->type],bptr->index);
                    if (!rptr->dead_because_of_bomb) action_lst.push_back(ta);
                    rptr->dead_because_of_bomb = true,bptr->dead_because_of_bomb = true;
                    rptr->Bomb();
                }
            }
            if ((war_map[i].flag == -1) || (war_map[i].flag == 0 && i % 2 == 0)) {
                // 蓝武士主动攻击
                int blue_atk = bptr->attack_value + bptr->get_sword_atk();
                if (rptr->life_value - blue_atk <= 0 && rptr->have_bomb()) {
                    rptr->life_value = -1,bptr->life_value = -1;
                    tank ta("red "+Map[rptr->type],rptr->index,"blue "+Map[bptr->type],bptr->index);
                    if (!rptr->dead_because_of_bomb) action_lst.push_back(ta);
                    rptr->dead_because_of_bomb = true,bptr->dead_because_of_bomb = true;
                    rptr->Bomb();
                }
            }
            // 蓝武士评估
            // 这里我理解的是如果红武士丢过了炸弹没必要再丢一遍？
            if ((war_map[i].flag == -1) || (war_map[i].flag == 0 && i % 2 == 0)) {
                // 蓝武士主动攻击
                int blue_atk = bptr->attack_value + bptr->get_sword_atk();
                int red_ctk = rptr->attack_value / 2 + rptr->get_sword_atk();
                if (rptr->life_value - blue_atk > 0 && bptr->life_value - red_ctk <= 0 && bptr->have_bomb()) {
                    bptr->life_value = -1, rptr->life_value = -1;
                    tank ta("blue " + Map[bptr->type],bptr->index,"red " + Map[rptr->type],rptr->index);
                    if (!bptr->dead_because_of_bomb) action_lst.push_back(ta);
                    bptr->dead_because_of_bomb = true, rptr->dead_because_of_bomb = true;
                    bptr->Bomb();
                }
            }
            if ((war_map[i].flag == 1) || (war_map[i].flag == 0 && i % 2 == 1)) {
                // 红武士主动攻击
                int red_atk = rptr->attack_value + rptr->get_sword_atk();
                if (bptr->life_value - red_atk <= 0 && bptr->have_bomb()) {
                    rptr->life_value = -1,bptr->life_value = -1;
                    tank ta("blue " + Map[bptr->type],bptr->index,"red " + Map[rptr->type],rptr->index);
                    if (!bptr->dead_because_of_bomb) action_lst.push_back(ta);
                    bptr->dead_because_of_bomb = true, rptr->dead_because_of_bomb = true;
                    bptr->Bomb();
                }
            }
        }
    }

    for (const tank& act : action_lst) {cout<< timePrefix(time_index) << act.war1 << " " << act.id1 << " used a bomb and killed " << act.war2 << " " << act.id2 << endl;}
    action_lst.clear();
}

/*
在有两个武士的城市，会发生战斗。 如果敌人在5分钟前已经被飞来的arrow射死，那么仍然视为发生了一场战斗，而且存活者视为获得了战斗的胜利
此情况下不会有“武士主动攻击”，“武士反击”，“武士战死”的事件发生
但战斗胜利后应该发生的事情都会发生。如Wolf一样能缴获武器，旗帜也可能更换，等等。在此情况下,Dragon同样会通过判断是否应该轮到自己主动攻击来决定是否欢呼
*/
struct event {
    int who_first=-3; // 0为蓝，1为红,-1为反击,-2为战死,2为dragon欢呼,3为武士获取生命元,4为旗帜升起
    int amount = -1;
    string redw="";
    int red_idx=-1;
    string bluew="";
    int blue_idx=-1;
    int locate=-1;
    int before_life=-1;
    int before_force=-1;
    event(int a1,const unique_ptr<Warrior>&rptr,const unique_ptr<Warrior>&bptr,int a2):who_first(a1),locate(a2){
        redw = rptr->side + " " + Map[rptr->type];red_idx = rptr->index;bluew = bptr->side + " " + Map[bptr->type];blue_idx = bptr->index;
        before_life = (who_first == 1) ? rptr->life_value : bptr->life_value;
        before_force = (who_first == 1) ? rptr->attack_value : bptr->attack_value;
    }
    event(int a1,const unique_ptr<Warrior>&ptr,int a2):who_first(a1),locate(a2) /*dragon欢呼*/ {
        redw = ptr->side + " " + Map[ptr->type] + " " + to_string(ptr->index);
    }
    event(int a1,string ptr,int a2,int loc):who_first(a1),amount(a2),locate(loc) /*武士获取生命元*/ {
        redw = ptr;
    }
    event(int a1,string str,int loc):who_first(a1),redw(str),locate(loc) /*旗帜升起*/ {}

    // event 的复制构造函数
    event(const event &other)
    : who_first(other.who_first),
        amount(other.amount),
        redw(other.redw),
        red_idx(other.red_idx),
        bluew(other.bluew),
        blue_idx(other.blue_idx),
        locate(other.locate),
        before_life(other.before_life),
        before_force(other.before_force)
    {}
};
struct element_aquire {
    string side="";
    string war=""; // 同时存储阵营，类型，编号
    int num=0;
    element_aquire(string str1,string str2,int n):side(str1),war(str2),num(n){}
    element_aquire(const element_aquire& ele) {
        side = ele.side,war = ele.war,num = ele.num;
    }
};
// ninja 挨打了也从不反击敌人
void battle(int time_index) {
    vector<int> red_reward; // 红方奖励列表，先进后出，存储的是位置，但生命值不一定发完
    queue<int> blue_reward; // 蓝方奖励列表，先进先出，存储的是位置，但生命值不一定发完

    vector<element_aquire> element_lst;
    vector<event> action_lst;
    for (int pos = 1; pos <= N ; pos ++) {
        auto &rptr = war_map[pos].red_warrior;
        auto &bptr = war_map[pos].blue_warrior;
        if (rptr && bptr) { // 有双方武士，但生死不明
            // lion 若是战死，则其战斗前的生命值就会转移到对手身上。所谓“战斗前”，就是每个小时的40分前的一瞬间。
            int red_before_element = (rptr->type == LION) ? rptr->life_value:0;
            int blue_before_element = (bptr->type == LION) ? bptr->life_value:0;
            if (rptr->life_value <= 0 && bptr->life_value <= 0) { //都死掉的情况不管是炸死的还是射死的都重新设置
                war_map[pos].red_warrior.reset();
                war_map[pos].blue_warrior.reset();
                continue;
            }

            war_map[pos].first_battle_result = war_map[pos].second_battle_result; // 更新战斗结果
            /*
            但是sword每经过一次战斗(不论是主动攻击还是反击)，就会变钝，
            攻击力变为本次战斗前的80% (去尾取整)。sword攻击力变为0时，视为武士失去了sword。如果武士降生时得到了一个初始攻击力为0的sword，则视为武士没有sword.
            */
            if (rptr->life_value > 0 && bptr->life_value > 0) { /*都活着就开打*/
                if ((war_map[pos].flag == 1) || (war_map[pos].flag == 0 && pos % 2 == 1)) { /*红武士先进攻*/
                    event e(1,rptr,bptr,pos);
                    if (rptr->type == DRAGON) rptr->dragon_attack_first = true;
                    bptr->life_value -= rptr->attack_value + rptr->get_sword_atk();
                    rptr->check_sword();
                    action_lst.push_back(e);
                }
                if ((war_map[pos].flag == -1) || (war_map[pos].flag == 0 && pos % 2 == 0)) {
                    event e(0,rptr,bptr,pos);
                    if (bptr->type == DRAGON) bptr->dragon_attack_first = true;
                    rptr->life_value -= bptr->attack_value + bptr->get_sword_atk();
                    bptr->check_sword();
                    action_lst.push_back(e);
                }

                if (rptr->life_value > 0 && bptr->life_value > 0) { // 反击
                    if (action_lst.back().who_first == 1 && bptr->type != NINJA) /*轮到蓝方反击*/ { 
                        event e(-1,bptr,rptr,pos);
                        rptr->life_value -= bptr->attack_value / 2 + bptr->get_sword_atk();
                        bptr->check_sword();
                        action_lst.push_back(e);
                    }
                    if (action_lst.back().who_first == 0 && rptr->type != NINJA) /*轮到红方反击*/ {
                        event e(-1,rptr,bptr,pos);
                        bptr->life_value -= rptr->attack_value / 2 + rptr->get_sword_atk();
                        rptr->check_sword();
                        action_lst.push_back(e);
                    }
                }
                /*
                lion 有“忠诚度”这个属性，其初始值等于它降生之后其司令部剩余生命元的数目。每经过一场未能杀死敌人的战斗，忠诚度就降低K
                */

                /*
                如果武士在战斗中杀死敌人（不论是主动进攻杀死还是反击杀死）
                则其司令部会立即向其发送8个生命元作为奖励，使其生命值增加8。当然前提是司令部得有8个生命元。如果司令部的生命元不足以奖励所有的武士，则优先奖励距离敌方司令部近的武士

                如果某武士在某城市的战斗中杀死了敌人，则该武士的司令部立即取得该城市中所有的生命元。
                注意，司令部总是先完成全部奖励工作，然后才开始从各个打了胜仗的城市回收生命元。对于因司令部生命元不足而领不到奖励的武士，司令部也不会在取得战利品生命元后为其补发奖励

                如果一次战斗的结果是双方都幸存(平局)，则双方都不能拿走发生战斗的城市的生命元
                */
            }
            // 下面的代码包含了35分钟就被射死的情况，易知如果最开始两个都没死的话,到这步最少有一个是活的
            if (rptr->life_value <= 0 || bptr->life_value <= 0) /*武士战死*/ {
                if (rptr->life_value <= 0) { // 蓝方胜利
                    war_map[pos].second_battle_result = -1;
                    bptr->life_value += red_before_element;
                    if (bptr->type == DRAGON) bptr->morale += 0.2;
                    if (bptr->type == WOLF) { // 这里认为被箭射死也算杀死敌人
                        // 把 bptr 视作 Wolf*，使用 dynamic_cast 并做空指针检查以保证安全
                        Wolf* wolf = dynamic_cast<Wolf*>(bptr.get());
                        if (wolf) {
                            vector<Weapon> bag = rptr->pass_weapon();
                            // 只缴获 wolf 自己没有的武器类型（按类型判断），并放入第一个空槽
                            for (const Weapon &w : bag) {
                                if (w.id == -1) continue; // 无效武器跳过
                                // 若 wolf 已有同类型武器，则跳过
                                if (wolf->wp1.id == w.id || wolf->wp2.id == w.id || wolf->wp3.id == w.id) continue;
                                // 否则放入第一个空槽
                                if (wolf->wp1.id == -1) { wolf->wp1 = w; }
                                else if (wolf->wp2.id == -1) { wolf->wp2 = w; }
                                else if (wolf->wp3.id == -1) { wolf->wp3 = w; }
                                // 若没有空槽，则跳过该武器
                            }
                            // 更新拥有武器标志
                            wolf->have_weapon = wolf->check_weapon();
                        }
                    }
                    if (!rptr->beaten_by_arrow) {
                        blue_reward.push(pos);
                        element_aquire ele("blue","blue "+Map[war_map[pos].blue_warrior->type]+" "+to_string(war_map[pos].blue_warrior->index),war_map[pos].out_put);
                        war_map[pos].out_put = 0;
                        element_lst.push_back(ele);
                        event e(-2,rptr,bptr,pos);
                        action_lst.push_back(e);
                        war_map[pos].red_warrior.reset();
                        war_map[pos].blue_warrior->beaten_by_arrow = false;
                    }
                }
                if (bptr->life_value <= 0) { // 红方胜利
                    rptr->life_value += blue_before_element;
                    war_map[pos].second_battle_result = 1;
                    if (rptr->type == DRAGON) rptr->morale += 0.2;
                    if (rptr->type == WOLF) {
                        Wolf* wolf = dynamic_cast<Wolf*>(rptr.get());
                        if (wolf) {
                            vector<Weapon> bag = bptr->pass_weapon();
                            for (const Weapon &w : bag) {
                                if (w.id == -1) continue;
                                if (wolf->wp1.id == w.id || wolf->wp2.id == w.id || wolf->wp3.id == w.id) continue;
                                if (wolf->wp1.id == -1) { wolf->wp1 = w;}
                                else if (wolf->wp2.id == -1) { wolf->wp2 = w;}
                                else if (wolf->wp3.id == -1) { wolf->wp3 = w;}
                            }
                            wolf->have_weapon = wolf->check_weapon();
                        }
                    }
                    if (!bptr->beaten_by_arrow) {
                        red_reward.push_back(pos);
                        element_aquire ele("red","red "+Map[war_map[pos].red_warrior->type]+" "+to_string(war_map[pos].red_warrior->index),war_map[pos].out_put);
                        war_map[pos].out_put = 0;
                        element_lst.push_back(ele);
                        event e(-2,bptr,rptr,pos);
                        action_lst.push_back(e);
                        war_map[pos].blue_warrior.reset();
                        war_map[pos].red_warrior->beaten_by_arrow = false;
                    }
                }
            }
            else if (rptr->life_value > 0 && bptr->life_value > 0) /*幸存*/ { //平局
                war_map[pos].second_battle_result = 0;
                if (rptr->type == LION) rptr->loyality -= K;
                if (bptr->type == LION) bptr->loyality -= K;
                if (rptr->type == DRAGON) rptr->morale -= 0.2;
                if (bptr->type == DRAGON) bptr->morale -= 0.2;
            }
// dragon 在一次在它主动进攻的战斗结束后，如果还没有战死，而且士气值大于0.8，就会欢呼
            if (rptr) {
                if (rptr->dragon_attack_first) {
                    rptr->dragon_attack_first = false;
                    if (rptr->morale > 0.8) {
                        event e(2,rptr,pos);
                        action_lst.push_back(e);
                    }
                }
            }
            if (bptr) {
                if (bptr->dragon_attack_first) {
                    bptr->dragon_attack_first = false;
                    if (bptr->morale > 0.8) {
                        event e(2,bptr,pos);
                        action_lst.push_back(e);
                    }
                }
            }
// 如果某武士在某城市的战斗中杀死了敌人，则该武士的司令部立即取得该城市中所有的生命元。注意，司令部总是先完成全部奖励工作，然后才开始从各个打了胜仗的城市回收生命元
            if (war_map[pos].second_battle_result != 0) {
                element_aquire temp = element_lst.back();
                event e(3,temp.war,temp.num,pos);
                action_lst.push_back(e);
            }
// 旗帜升起
            if (war_map[pos].first_battle_result == war_map[pos].second_battle_result) {
                if (war_map[pos].first_battle_result == 1) {
                    event e(4,"red",pos);
                    action_lst.push_back(e);
                }
                if (war_map[pos].second_battle_result == -1) {
                    event e(4,"blue",pos);
                    action_lst.push_back(e);
                }
            }
        }
        else { // 需要更新只有一个武士但被射死的指针
            if (rptr) {
                if (rptr->life_value <= 0) war_map[pos].red_warrior.reset();
            }
            if (bptr) {
                if (bptr->life_value <= 0) war_map[pos].blue_warrior.reset();
            }
        }
    }

    while (!red_reward.empty()) {
        if (redhq.life < 8) break;
        int pos = red_reward.back();red_reward.pop_back();
        redhq.life -= 8;
        war_map[pos].red_warrior->life_value += 8;
    }
    while (!blue_reward.empty()) {
        if (bluehq.life < 8) break;
        int pos = blue_reward.front();blue_reward.pop();
        bluehq.life -= 8;
        war_map[pos].blue_warrior->life_value += 8;
    }
    for (const element_aquire& e:element_lst) {
        if (e.side == "red") redhq.life += e.num;
        else bluehq.life += e.num;
    }

    for (const event& e:action_lst) {
        switch (e.who_first) {
            case 1 : cout<<timePrefix(time_index)<<e.redw<<" "<<e.red_idx<<" "<<"attacked "<<e.bluew<<" "<<e.blue_idx<<" "<<"in city "<<e.locate<<" with "<<e.before_life<<" elements and force "<<e.before_force<<endl;break;
            case 0 : cout<<timePrefix(time_index)<<e.bluew<<" "<<e.blue_idx<<" "<<"attacked "<<e.redw<<" "<<e.red_idx<<" "<<"in city "<<e.locate<<" with "<<e.before_life<<" elements and force "<<e.before_force<<endl;break;
            case -1 : cout<<timePrefix(time_index)<<e.redw<<" "<<e.red_idx<<" fought back against "<<e.bluew<<" "<<e.blue_idx<<" in city "<<e.locate<<endl;break;
            case -2 : cout<<timePrefix(time_index)<<e.redw<<" "<<e.red_idx<<" was killed in city "<<e.locate<<endl;break;
            case 2 : cout<<timePrefix(time_index)<<e.redw<<" yelled in city "<<e.locate<<endl;break;
            case 3 : cout<<timePrefix(time_index)<<e.redw<<" earned "<<e.amount<<" elements for his headquarter"<<endl;break;
            case 4 : cout<<timePrefix(time_index)<<e.redw<<" flag raised in city "<<e.locate<<endl;break;
        }
    }
}

void head_report(int time_index) {
    cout<<timePrefix(time_index)<<redhq.life<<" elements in red headquarter"<<endl;
    cout<<timePrefix(time_index)<<bluehq.life<<" elements in blue headquarter"<<endl;
}

void warrior_report(int time_index) {
    for (int pos=0;pos<=N+1;pos++) {
        if (war_map[pos].red_warrior) {
            auto &w = war_map[pos].red_warrior;
            vector<Weapon> bag = w->pass_weapon();
            cout<<timePrefix(time_index)<<"red "<<Map[w->type]<<" "<<w->index<<" has ";
            vector<pair<string,int>> out_inf;
            for (size_t num=0;num<bag.size();num++) {if (bag[num].id == ARROW) out_inf.push_back(make_pair("arrow",3 - bag[num].arrow_cnt));}
            for (size_t num=0;num<bag.size();num++) {if (bag[num].id == BOMB) out_inf.push_back(make_pair("bomb",-1));}
            for (size_t num=0;num<bag.size();num++) {if (bag[num].id == SWORD) out_inf.push_back(make_pair("sword",bag[num].sword_atk));}
            if (out_inf.empty()) {
                cout<<"no weapon"<<endl;
                continue;
            }
            for (size_t len=0;len<out_inf.size();len++) {
                cout<<out_inf[len].first;
                if (out_inf[len].second > 0) cout<<"("<<out_inf[len].second<<")";
                if (len + 1 < out_inf.size()) cout<<",";
            }
            cout<<endl;
        }
    }
    for (int pos=0;pos<=N+1;pos++) {
        if (war_map[pos].blue_warrior) {
            auto &w = war_map[pos].blue_warrior;
            vector<Weapon> bag = w->pass_weapon();
            cout<<timePrefix(time_index)<<"blue "<<Map[w->type]<<" "<<w->index<<" has ";
            vector<pair<string,int>> out_inf;
            for (size_t num=0;num<bag.size();num++) {if (bag[num].id == ARROW) out_inf.push_back(make_pair("arrow",3 - bag[num].arrow_cnt));}
            for (size_t num=0;num<bag.size();num++) {if (bag[num].id == BOMB) out_inf.push_back(make_pair("bomb",-1));}
            for (size_t num=0;num<bag.size();num++) {if (bag[num].id == SWORD) out_inf.push_back(make_pair("sword",bag[num].sword_atk));}
            if (out_inf.empty()) {
                cout<<"no weapon"<<endl;
                continue;
            }
            for (size_t len=0;len<out_inf.size();len++) {
                cout<<out_inf[len].first;
                if (out_inf[len].second > 0) cout<<"("<<out_inf[len].second<<")";
                if (len + 1 < out_inf.size()) cout<<",";
            }
            cout<<endl;
        }
    }
}

int main() {
    int tt;
    cin>>tt;
    const bool kCompareOutput = true;
    if (kCompareOutput) {
        vector<string> expected = loadExpectedLines("warfinaldatapub/data.out");
        if (expected.empty()) {
            cerr << "[COMPARE] expected output is empty or missing\n";
        } else {
            static CompareBuf compareBuf(cout.rdbuf(), std::move(expected));
            cout.rdbuf(&compareBuf);
        }
    }
    for (int tc=0;tc<tt;tc++) {
        cin>>M>>N>>R>>K>>T;
        for (int iter=0;iter<5;iter++) cin>>cost[iter];
        for (int iter=0;iter<5;iter++) cin>>attack[iter];
        redhq = Headquarters("red");
        bluehq = Headquarters("blue");

        war_map.resize(N+2);

        cout<<"Case "<< tc + 1 <<":"<<endl;

        int time_index = 0;
        while (true) {
            int curMinutes = (time_index / 10) * 60 + timeOrder[time_index % 10];
            if (curMinutes > T) break;
            int slot = time_index % 10;
            if (slot == 0) {
                makeWarrior(redhq, redOrder, time_index);
                makeWarrior(bluehq, blueOrder, time_index);
            }
            else if (slot == 1) {
                for (int pos = 0;pos <= N+1; ++pos) {
                    lion_escape(redhq, time_index, pos);
                    lion_escape(bluehq, time_index, pos);
                }
            }
            else if (slot == 2) {
                Warrior_March(time_index);
                if (redhq.occupy || bluehq.occupy) break;
            }
            else if (slot == 3) {city_output(time_index);}
            else if (slot == 4) {fetch_element(time_index);}
            else if (slot == 5) {throw_arrow(time_index);}
            else if (slot == 6) {use_bomb(time_index);}
            else if (slot == 7) {battle(time_index);}
            else if (slot == 8) {head_report(time_index);}
            else if (slot == 9) {warrior_report(time_index);}

            ++time_index;
        }
    }
    return 0;
}
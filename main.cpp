#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <sstream>

using namespace std;

//#define DEBUG_TRACE_FUNCTION //启用函数追踪

int popspace(){
    while(cin.peek()==' ')
        cin.get();
    return cin.peek();
}
//判断ch是否为合法标识符
bool validch(int ch){
    const static char str[]="+-*/!?=<>_";
    if(ch>='A'&&ch<='Z')
        return true;
    if(ch>='a'&&ch<='z')
        return true;
    if(ch>='0'&&ch<='9')
        return true;
    int i=0;
    while(str[i]&&str[i]!=ch)
        i++;
    return str[i]!='\0';
}
//当发生错误时调用此函数
void err(string str){
    cerr<<"ERR:"<<str<<endl;
    exit(0);
}
//表达式基类
class Expr{
public:
    //表达式计算
    virtual Expr * val(){
        err("invalid expr");
        return nullptr;
    }
    //表达式的值
    virtual string value(){
        err("undefined value");
        return "ERROR";
    }
};
//上下文环境
class Env{
public:
    Env * upward;
    map<string,Expr *> context;

    Env(Env * upward):upward(upward){}

    void push(string name,Expr * c){
        context[name]=c;
    }
    Expr * get(string str){
        if(context.count(str)==1)
            return context[str];
        else{
            if(upward == nullptr)
                return nullptr;
            return upward->get(str);
        }
    }
};

Env * globalEnv;//全局环境，所有环境均指向globalEnv
Env * curEnv;//当前使用的运行时环境
//原子类型，用作标识符，需要从当前运行时环境取得具体常量值
class E_Atom:public Expr{
public:
    string val_str;

    E_Atom(string s):val_str(s){}

    virtual Expr * val(){
        Expr * ret = curEnv->get(val_str);
        if(ret == nullptr){
            cerr<<"atom \""<<val_str<<"\" error. env:"<<(curEnv)<<" size:"<<curEnv->context.size()<<endl;
            err("invalid atom");
        }
        return ret;
    }
    virtual string value(){
        return val_str;
    }
};
//int类型
class E_int:public Expr{
public:
    int val_int;

    E_int(string s){
        stringstream ss;
        ss<<s;
        ss>>val_int;
    }
    E_int(int s):val_int(s){}

    virtual Expr * val(){
        return this;
    }

    virtual string value(){
        stringstream ss;
        ss<<val_int;
        return ss.str();
    }
};

class E_bool:public Expr{
public:
    bool val_bool;
    E_bool(bool b):val_bool(b){}
    virtual Expr * val(){
        return this;
    }
    virtual string value(){
        return val_bool?"True":"False";
    }
};

E_bool e_true(true),e_false(false);

class E_list;
//lambda表达式，包含几个内置函数
class E_lambda:public Expr{
public:
    Env * bindenv;

    vector<E_Atom *> arg;
    E_list * body;

    virtual Expr * val(){
        return this;
    }
    virtual Expr * getval();

    virtual string value(){
        err("lambda has no value");
        return "this is a lambda";
    }
};
//2参数变量内置函数，持有内置函数的函数指针
class L_Pre:public E_lambda{
private:
    static E_Atom v1,v2;
public:
    Expr *(*func)(Expr *,Expr *);
    L_Pre(Expr *(*func)(Expr *,Expr *)):func(func){
        arg.push_back(&v1);
        arg.push_back(&v2);
        bindenv = nullptr;
    }
    virtual Expr * getval(){
        return func(curEnv->get("v1"),curEnv->get("v2"));
    }
};
//内置define函数(第一个参数为原始Atom，第二个参数传值)
class L_DEF:public L_Pre{
public:
    L_DEF(Expr *(*func)(Expr *,Expr *)):L_Pre(func){

    }
};
//内置lambda函数(两个参数均为原始Atom，且保留当前运行时环境)
class L_LAMBDA:public L_Pre{
public:
    L_LAMBDA(Expr *(*func)(Expr *,Expr *)):L_Pre(func){
    }
};
//内置cond函数，不定参数，每个参数都是一个原始Atom(理论上必须为list)
class L_COND:public E_lambda{
    virtual Expr * getval();
};
//list列表
class E_list:public Expr{
public:
    vector<Expr *> val_list;
//对list列表求值，根据第一个元素走不同的流程
    virtual Expr * val(){
        Expr * ret = val_list[0]->val();
        E_lambda * lmd = dynamic_cast<E_lambda *>(ret);//列表第一个需要是lambda函数，否则不可执行
        if(lmd==nullptr)
            err("invalid function");

#ifdef DEBUG_TRACE_FUNCTION
        //运行时追踪函数轨迹并输出至cout
        E_Atom * tempat = dynamic_cast<E_Atom *>(val_list[0]);

        if(tempat != nullptr){
            cout<<"do "<<tempat->val_str<<' ';
        }
        else
            cout<<"do unknown"<<endl;
#endif //DEBUG_TRACE_FUNCTION

        Env * oldenv = curEnv;
        Env * newenv;

        if(dynamic_cast<L_Pre *>(ret)!= nullptr || dynamic_cast<L_COND *>(ret)!= nullptr){
            //cond和预置函数的上下文环境指向当前环境
            newenv = new Env(curEnv);

#ifdef DEBUG_TRACE_FUNCTION
            //函数执行时输出函数运行时环境地址至cout
            cout<<"last pre function,new env:"<<newenv<<" to "<<newenv->upward<<endl;
#endif //DEBUG_TRACE_FUNCTION

        }else{
            newenv = new Env(lmd->bindenv);//将新的上下文环境设置为指向lambda创建时环境的新环境
        }

        if(dynamic_cast<L_COND *>(ret)!= nullptr){//cond为不定数量参数，做特殊处理
            char str[20];
            str[0]='v';
            for(size_t i=1;i<val_list.size();i++){
                sprintf(str+1,"%ud",i);
                newenv->push(str,val_list[i]);//将val_list[i]映射到v1 v2 v3 ...
            }
        }else{
            for(size_t i=0;i<lmd->arg.size();i++){
                //将val_list[i+1]映射到lmd->arg[i]
                if((i == 0 &&(dynamic_cast<L_DEF *>(ret) != nullptr))||//define 只传一个list
                        (dynamic_cast<L_LAMBDA *>(ret) != nullptr)// lambda全部传递list
                        )
                    newenv->push(lmd->arg[i]->val_str,val_list[i+1]);//传递原始Atom
                else
                    newenv->push(lmd->arg[i]->val_str,val_list[i+1]->val());//传递值
            }
        }
        curEnv = newenv;
        ret = lmd->getval();
        curEnv = oldenv;//恢复环境
        return ret;
    }

    virtual string value(){
        return val()->value();
    }
};
//内置两变量函数参数名
E_Atom L_Pre::v1("v1");
E_Atom L_Pre::v2("v2");

Expr * E_lambda::getval(){
    return body->val();
}

//从cin构造表达式，类似语法制导
Expr * readexpr();
//从cin构造list
E_list * readlist(){
    E_list * ret = new E_list();

    cin.get();//pop (
    popspace();
    while(cin.peek()!=')'){
        ret->val_list.push_back(readexpr());
        popspace();
    }
    cin.get();//pop )
    return ret;
}

Expr * readexpr(){
    popspace();
    if(cin.peek() == '(')
        return readlist();
    else{
        string str;
        while(validch(cin.peek()))
            str+=cin.get();
        if(str == "True")
            return &e_true;
        else if(str == "False")
            return &e_false;
        for(size_t i = 0;i<str.size();i++){
            if(str[i]>'9'||str[i]<'0')
                return new E_Atom(str);
        }
        return new E_int(str);
    }
}
//几个内置函数
Expr * L_plus(Expr *a,Expr *b){
    E_int   *x = dynamic_cast<E_int *>(a),
            *y = dynamic_cast<E_int *>(b);
    if(x == nullptr || y == nullptr){
        err("invalid plus operation");
        return nullptr;
    }
    return new E_int(x->val_int + y->val_int);
}

Expr * L_sub(Expr *a,Expr *b){
    E_int   *x = dynamic_cast<E_int *>(a),
            *y = dynamic_cast<E_int *>(b);
    if(x == nullptr || y == nullptr){
        err("invalid plus operation");
        return nullptr;
    }
    return new E_int(x->val_int - y->val_int);
}
Expr * L_mul(Expr *a,Expr *b){
    E_int   *x = dynamic_cast<E_int *>(a),
            *y = dynamic_cast<E_int *>(b);
    if(x == nullptr || y == nullptr){
        err("invalid plus operation");
        return nullptr;
    }
    return new E_int(x->val_int * y->val_int);
}

Expr * L_div(Expr *a,Expr *b){
    E_int   *x = dynamic_cast<E_int *>(a),
            *y = dynamic_cast<E_int *>(b);
    if(x == nullptr || y == nullptr){
        err("invalid plus operation");
        return nullptr;
    }
    return new E_int(x->val_int / y->val_int);
}

Expr * L_eq(Expr *a,Expr *b){
    E_bool  *b1 = dynamic_cast<E_bool *>(a),
            *b2 = dynamic_cast<E_bool *>(b);
    E_int   *i1 = dynamic_cast<E_int * >(a),
            *i2 = dynamic_cast<E_int * >(b);
    if(b1 != nullptr && b2 != nullptr){
        //cout<<"eq:"<<(b1->val_bool == b2->val_bool)<<' '<<endl;
        return b1->val_bool == b2->val_bool ? &e_true : &e_false;
    }
    if(i1 != nullptr && i2 != nullptr){
        //cout<<"eq:"<<(i1->val_int == i2->val_int)<<' '<<endl;
        return i1->val_int == i2->val_int ? &e_true : &e_false;
    }
    err("invalid eq type");
    return nullptr;
}

E_Atom e_define("define");

Expr * L_define(Expr *a,Expr *b){
    E_Atom *x = dynamic_cast<E_Atom*>(a);
    if(x == nullptr){
        err("invalid define(v1 is not an atom)");
        return nullptr;
    }
    globalEnv->push(x->value(),b->val());
    return &e_define;
}

Expr * L_lambda(Expr *a,Expr *b){
    E_lambda * ret = new E_lambda();
    E_list * x1 = dynamic_cast<E_list *>(a),
            * x2 = dynamic_cast<E_list *>(b);
    if(x1==nullptr||x2==nullptr)
        err("invalid lambda arguments or body");
    for(size_t i=0;i<x1->val_list.size();i++){
        E_Atom * ad = dynamic_cast<E_Atom *>(x1->val_list[i]);
        if(ad==nullptr)
            err("invalid lambda arguments");
        ret->arg.push_back(ad);
    }
    ret->bindenv = curEnv->upward;//保存在lambda创建时的上下文环境
    ret->body = x2;
    return ret;
}

Expr * L_COND::getval(){
    size_t i=1;
    char str[20];
    str[0]='v';
    while(1){
        sprintf(str+1,"%ud",i);
        E_list *p = dynamic_cast<E_list *>(curEnv->get(str));
        if(p == nullptr)
            err("invalid arg in cond");
        E_bool *b = dynamic_cast<E_bool *>(p->val_list[0]->val());
        if(b == nullptr)
            err("invalid bool type in cond");
        if(b->val_bool)
            return p->val_list[1]->val();
        i++;
    }
}

int main()
{
    //初始化全局环境
    globalEnv = new Env(nullptr);
    curEnv = globalEnv;
    //将预置函数加入全局环境
    globalEnv->push("+",new L_Pre(L_plus));
    globalEnv->push("-",new L_Pre(L_sub));
    globalEnv->push("*",new L_Pre(L_mul));
    globalEnv->push("/",new L_Pre(L_div));
    globalEnv->push("eq?",new L_Pre(L_eq));
    globalEnv->push("cond",new L_COND());
    globalEnv->push("define",new L_DEF(L_define));
    globalEnv->push("lambda",new L_LAMBDA(L_lambda));

    //cout<<globalEnv<<' '<<globalEnv->context.size()<<endl;
    cout<<">";
    while(popspace(),cin){
        cout<<readexpr()->value()<<endl;
        cin.get();// pop \n
        cout<<">";
    }

    return 0;
}

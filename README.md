
# FFRPC

FFRPC 已经陆陆续续开发了1年，6月6日这天终于完成了我比较满意的版本，暂称之为 V0.2，FFRPC实现了一个C++版本
的异步进程间通讯库。我本身是做游戏服务器程序的，在服务器程序领域，系统是分布式的，各个节点需要异步的进行通信，
我的初衷是开发一个易用、易测试的进程间socket通信组件。实际上FFRPC 已经是一个框架。

## FFRPC 主要特性
 * FFRPC 采用Epoll Edge Trigger模式，这里特别提一下ET是因为在异步工作模式，ET方式才是epoll最简单也是最高效的方式
 * 网上的很多帖子写LT简单易用，那纯碎是没有理解ET的精髓之所在，如果读者想要从ffrpc中探究一下ET的奥妙，提醒读者的是
 * 请把Epoll 看成一个状态机！
 * FFRPC 采用Broker模式，这样的好处是 Scalability!! 在游戏领域的开发者一定很熟悉Master/Gateway/Logic Server的概念，
 * 实际上Master 实际上扮演的Broker master的角色，而gateway扮演的是Broker slave的角色，Broker Slave负责转发客户端的
 * 请求到Logic Service，提供一个转发层虽然会增加延迟，但是系统变得可扩展，大大提高了吞吐量，这就是Scalability!! 
 * 而Broker master负责管理所有的Master Slave，负责负载均衡。不同的client分配不同的Broker SLave。
 * FFRPC 就是基于以上的思路，有如下四个关键的概念：
 * 一：broker master 负责负载均衡，同步所有节点的信息，所有的slave broker和rpc service/ rpc cleint都要连接broker master。
 * 二：slave broker负责完成service和client间转发消息，如果service、client和broker在同一进程，那么直接在内存间投递消息，
 * 这是v0。2的重要的优化，v0。1时没有此功能，网友很多反应这个问题，看来大伙对优化还是太敏感！
 * 另一个创新之处在于ffmsg_t，封装了消息的序列化和反序列化，我已经厌倦了protobuff，如果你也研究了为每个消息定义cmd
 * 和为cmd写switch（有些人可能已经用上注册回调函数，但还有更好用的）。实际上定义消息结构体时一个消息本身就是独一无二的，
 * 所以为什么我们还要给消息在定义一个cmd呢？比如定义了struct echo_t{int a;}消息，echo_t名称本身就是独一无二的，否则编译
 * 器肯定报错了，那么为什么不直接用echo_t这个名称作为cmd呢？在FFRPC中可以使用TYPE_NAME(echo_t)获得消息体名称字符串，
 * 是滴TYPE_NAME是一个很有意思的实现，c++中并没哟关键字可以获取一个类的名称，但是所有的编译器都实际上已经提供了这个功能！
 * 详情请看源码。有读者可能会纠结使用消息体结构的名称做cmd固然省事，但是浪费了流量！32位的cmd总是比字符串省流量，是的这个
 * 结论虽然我很不喜欢（我总是懒的优化，除非...被逼的），但是他是对的！ffrpc中很好的解决了这个问题，当每个节点初始化时都要
 * 注册到broker master，这时所有的消息都会在master中分配一个唯一的msg id，这样就可以用整数1代表echo_t结构了，由于每个节点
 * 都知道echo_t到1的映射，所以程序员再也不用手动定义cmd了，broker唯一初始化时动态定义。
 * 三:ffrpc service，提供接口的模块，也就就是服务端，通过ffrpc类注册的接口基于异步模式，推荐的模式是每个消息都返回
 * 一个结果消息
 * 四：ffrpc client是调用的ffrpc service的模块，基于异步模式，记住服务名成和消息名称唯一的确定一个接口，这个c++的类和类接口
 * 概念是一致的，而且调用远程接口时可以指定回调函数，而且回调函数还支持lambda参数绑定！
 * 想快速见证ffrpc库的魅力可以小看如下的示例，只要你有linux系统，可以1分钟内测试这个示例，ffrpc没有其他依赖，提醒你的是
 * FFRPC的日志组件是彩色的哦！
 
## Supported Python versions
 * python2.5 python2.6 python2.7, win / linux
 * python3.x is being developed, but unfortunately， python3.x api is so different to python2.x, even diffent between python3.2
  and python3.3, Headache!!

## Embed Python script in C++
### Get / Set varialbe in  python script/module
``` c++
	printf("sys.version=%s\n", ffpython.get_global_var<string>("sys", "version").c_str());
    ffpython.set_global_var("fftest", "global_var", "OhNice");
    printf("fftest.global_var=%s\n", ffpython.get_global_var<string>("fftest", "global_var").c_str());
```
### call python function, Support all base type as arg or return value. Nine args can be supported.
``` c++
	int a1 = 100; float a2 = 3.14f; string a3 = "OhWell";
    ffpython.call<void>("fftest", "test_base", a1, a2, a3);
```
### call python function, Support all STL type as arg or return value. Nine args can be supported. Vector and List for tuple and list in python,map for dict in python.
``` c++
	vector<int> a1;a1.push_back(100);a1.push_back(200);
    list<string> a2; a2.push_back("Oh");a2.push_back("Nice");
    vector<list<string> > a3;a3.push_back(a2);
    
    ffpython.call<bool>("fftest", "test_stl", a1, a2, a3);
	typedef map<string, list<vector<int> > > ret_t;
    ret_t val = ffpython.call<ret_t>("fftest", "test_return_stl");
```
## Extend Python
### register c++ static function, all base type supported. Arg num can be nine.
``` c++
static int print_val(int a1, float a2, const string& a3, const vector<double>& a4)
{
    printf("%s[%d,%f,%s,%d]\n", __FUNCTION__, a1, a2, a3.c_str(), a4.size());
    return 0;
}
struct ops_t
{
    static list<int> return_stl()
    {
        list<int> ret;ret.push_back(1024);
        printf("%s\n", __FUNCTION__);
        return ret;
    }
};

void test_reg_function()
{
    ffpython_t ffpython;//("ext1");
    ffpython.reg(&print_val, "print_val")
            .reg(&ops_t::return_stl, "return_stl");
    ffpython.init("ext1");
    ffpython.call<void>("fftest", "test_reg_function");
}
```
### register c++ class, python can use it just like builtin types.
``` c++

class foo_t
{
public:
	foo_t(int v_):m_value(v_)
	{
		printf("%s\n", __FUNCTION__);
	}
    virtual ~foo_t()
    {
        printf("%s\n", __FUNCTION__);
    }
	int get_value() const { return m_value; }
	void set_value(int v_) { m_value = v_; }
	void test_stl(map<string, list<int> >& v_) 
	{
		printf("%s\n", __FUNCTION__);
	}
	int m_value;
};

class dumy_t: public foo_t
{
public:
    dumy_t(int v_):foo_t(v_)
    {
        printf("%s\n", __FUNCTION__);
    }
    ~dumy_t()
    {
        printf("%s\n", __FUNCTION__);
    }
    void dump() 
    {
        printf("%s\n", __FUNCTION__);
    }
};


static foo_t* obj_test(dumy_t* p)
{
    printf("%s\n", __FUNCTION__);
    return p;
}

void test_register_base_class(ffpython_t& ffpython)
{
	ffpython.reg_class<foo_t, PYCTOR(int)>("foo_t")
			.reg(&foo_t::get_value, "get_value")
			.reg(&foo_t::set_value, "set_value")
			.reg(&foo_t::test_stl, "test_stl")
            .reg_property(&foo_t::m_value, "m_value");

    ffpython.reg_class<dumy_t, PYCTOR(int)>("dumy_t", "dumy_t class inherit foo_t ctor <int>", "foo_t")
        .reg(&dumy_t::dump, "dump");

    ffpython.reg(obj_test, "obj_test");

    ffpython.init("ext2");
    ffpython.call<void>("fftest", "test_register_base_class");
};
```
### Register c++ class which inherit a class having been registered.
``` c++	
ffpython.call<void>("fftest", "test_register_inherit_class");
```
### C++ object pointer can be as a arg to python, and object can be access as a instance of builtin type in python.
``` c++	
void test_cpp_obj_to_py(ffpython_t& ffpython)
{
    foo_t tmp_foo(2013);
    ffpython.call<void>("fftest", "test_cpp_obj_to_py", &tmp_foo);
}
void test_cpp_obj_py_obj(ffpython_t& ffpython)
{
    dumy_t tmp_foo(2013);
    
    foo_t* p = ffpython.call<foo_t*>("fftest", "test_cpp_obj_py_obj", &tmp_foo);
}
```
## Python test script
``` python
def test_base(a1, a2, a3):
	print('test_base', a1, a2, a3)
	return 0

def test_stl(a1, a2, a3):
	print('test_stl', a1, a2, a3)
	return True

def test_return_stl():
	print('test_return_stl')
	#map<string, list<vector<int> > >
	ret = {'Oh':[[111,222], [333, 444] ] }
	return ret

def test_reg_function():
	import ext1
	ext1.print_val(123, 45.6 , "----789---", [3.14])
	ret = ext1.return_stl()
	print('test_reg_function', ret)

def test_register_base_class():
	import ext2
	foo = ext2.foo_t(20130426)
	print("test_register_base_class get_val:", foo.get_value())
	foo.set_value(778899)
	print("test_register_base_class get_val:", foo.get_value(), foo.m_value)
	foo.test_stl({"key": [11,22,33] })
	print('test_register_base_class test_register_base_class', foo)

def test_register_inherit_class():
	import ext2
	dumy = ext2.dumy_t(20130426)
	print("test_register_inherit_class get_val:", dumy.get_value())
	dumy.set_value(778899)
	print("test_register_inherit_class get_val:", dumy.get_value(), dumy.m_value)
	dumy.test_stl({"key": [11,22,33] })
	dumy.dump()
	print('test_register_inherit_class', dumy)

def test_cpp_obj_to_py(foo):
	import ext2
	print("test_cpp_obj_to_py get_val:", foo.get_value())
	foo.set_value(778899)
	print("test_cpp_obj_to_py get_val:", foo.get_value(), foo.m_value)
	foo.test_stl({"key": [11,22,33] })
	print('test_cpp_obj_to_py test_register_base_class', foo)

def test_cpp_obj_py_obj(dumy):
	import ext2
	print("test_cpp_obj_py_obj get_val:", dumy.get_value())
	dumy.set_value(778899)
	print("test_cpp_obj_py_obj get_val:", dumy.get_value(), dumy.m_value)
	dumy.test_stl({"key": [11,22,33] })
	dumy.dump()
	ext2.obj_test(dumy)
	print('test_cpp_obj_py_obj', dumy)
	
	return dumy

``` 

## Summary
* ffpython Only One implement head file, it is easy to itegrate to project.
* ffpython is simplely wrap for python api, so it is efficient.



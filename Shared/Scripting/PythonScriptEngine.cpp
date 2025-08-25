#include "PythonScriptEngine.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace Helianthus::Scripting
{
    PythonScriptEngine::PythonScriptEngine()
    {
#ifdef ENABLE_PYTHON_SCRIPTING
        MainModule = nullptr;
        GlobalDict = nullptr;
#endif
    }

    PythonScriptEngine::~PythonScriptEngine()
    {
        Shutdown();
    }

    ScriptLanguage PythonScriptEngine::GetLanguage() const
    {
        return ScriptLanguage::Python;
    }

    ScriptResult PythonScriptEngine::Initialize()
    {
#ifdef ENABLE_PYTHON_SCRIPTING
        if (!InitializePython())
        {
            return {false, "Failed to initialize Python interpreter"};
        }
        return {true, ""};
#else
        return {false, "Python scripting is not enabled"};
#endif
    }

    void PythonScriptEngine::Shutdown()
    {
#ifdef ENABLE_PYTHON_SCRIPTING
        ShutdownPython();
#endif
    }

    ScriptResult PythonScriptEngine::LoadFile(const std::string& Path)
    {
#ifdef ENABLE_PYTHON_SCRIPTING
        if (!MainModule || !GlobalDict)
        {
            return {false, "Python interpreter not initialized"};
        }

        std::ifstream File(Path);
        if (!File.is_open())
        {
            return {false, "Cannot open file: " + Path};
        }

        std::stringstream Buffer;
        Buffer << File.rdbuf();
        std::string Code = Buffer.str();

        auto Result = ExecuteString(Code);
        if (Result.Success)
        {
            LoadedFileSet.insert(Path);
        }
        return Result;
#else
        return {false, "Python scripting is not enabled"};
#endif
    }

    ScriptResult PythonScriptEngine::ExecuteString(const std::string& Code)
    {
#ifdef ENABLE_PYTHON_SCRIPTING
        if (!MainModule || !GlobalDict)
        {
            return {false, "Python interpreter not initialized"};
        }

        PyObject* PyCode = Py_CompileString(Code.c_str(), "<string>", Py_file_input);
        if (!PyCode)
        {
            return {false, GetPythonError()};
        }

        PyObject* Result = PyEval_EvalCode(PyCode, GlobalDict, GlobalDict);
        Py_DECREF(PyCode);

        if (!Result)
        {
            return {false, GetPythonError()};
        }

        Py_DECREF(Result);
        return {true, ""};
#else
        return {false, "Python scripting is not enabled"};
#endif
    }

    ScriptResult PythonScriptEngine::CallFunction(const std::string& Name, const std::vector<std::string>& Args)
    {
#ifdef ENABLE_PYTHON_SCRIPTING
        if (!MainModule || !GlobalDict)
        {
            return {false, "Python interpreter not initialized"};
        }

        // 获取函数对象
        PyObject* Func = PyDict_GetItemString(GlobalDict, Name.c_str());
        if (!Func || !PyCallable_Check(Func))
        {
            return {false, "Function not found or not callable: " + Name};
        }

        // 创建参数元组
        PyObject* PyArgs = PyTuple_New(Args.size());
        for (size_t i = 0; i < Args.size(); ++i)
        {
            PyObject* PyArg = PyUnicode_FromString(Args[i].c_str());
            PyTuple_SetItem(PyArgs, i, PyArg);
        }

        // 调用函数
        PyObject* Result = PyObject_CallObject(Func, PyArgs);
        Py_DECREF(PyArgs);

        if (!Result)
        {
            return {false, GetPythonError()};
        }

        Py_DECREF(Result);
        return {true, ""};
#else
        return {false, "Python scripting is not enabled"};
#endif
    }

    ScriptResult PythonScriptEngine::ReloadFile(const std::string& Path)
    {
        // 从已加载文件集合中移除
        LoadedFileSet.erase(Path);
        
        // 重新加载文件
        return LoadFile(Path);
    }

    void PythonScriptEngine::SetHotReloadCallback(HotReloadCallback Callback)
    {
        HotReloadHandler = Callback;
    }

    bool PythonScriptEngine::IsFileLoaded(const std::string& Path) const
    {
        return LoadedFileSet.find(Path) != LoadedFileSet.end();
    }

    std::vector<std::string> PythonScriptEngine::GetLoadedFiles() const
    {
        return std::vector<std::string>(LoadedFileSet.begin(), LoadedFileSet.end());
    }

#ifdef ENABLE_PYTHON_SCRIPTING
    bool PythonScriptEngine::InitializePython()
    {
        // 初始化Python解释器
        Py_Initialize();
        
        if (!Py_IsInitialized())
        {
            return false;
        }

        // 获取主模块和全局字典
        MainModule = PyImport_AddModule("__main__");
        if (!MainModule)
        {
            return false;
        }

        GlobalDict = PyModule_GetDict(MainModule);
        if (!GlobalDict)
        {
            return false;
        }

        return true;
    }

    void PythonScriptEngine::ShutdownPython()
    {
        if (Py_IsInitialized())
        {
            Py_Finalize();
        }
        
        MainModule = nullptr;
        GlobalDict = nullptr;
    }

    std::string PythonScriptEngine::GetPythonError()
    {
        if (PyErr_Occurred())
        {
            PyObject* Type, *Value, *Traceback;
            PyErr_Fetch(&Type, &Value, &Traceback);
            
            std::string ErrorMsg;
            if (Value)
            {
                PyObject* Str = PyObject_Str(Value);
                if (Str)
                {
                    const char* ErrorStr = PyUnicode_AsUTF8(Str);
                    if (ErrorStr)
                    {
                        ErrorMsg = ErrorStr;
                    }
                    Py_DECREF(Str);
                }
                Py_DECREF(Value);
            }
            
            if (Type)
            {
                Py_DECREF(Type);
            }
            if (Traceback)
            {
                Py_DECREF(Traceback);
            }
            
            return ErrorMsg.empty() ? "Unknown Python error" : ErrorMsg;
        }
        
        return "No Python error";
    }
#else
    bool PythonScriptEngine::InitializePython()
    {
        return false;
    }

    void PythonScriptEngine::ShutdownPython()
    {
    }

    std::string PythonScriptEngine::GetPythonError()
    {
        return "Python scripting is not enabled";
    }
#endif

} // namespace Helianthus::Scripting

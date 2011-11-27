#ifndef CLANGPCHMAKERTHREAD_H
#define CLANGPCHMAKERTHREAD_H

#if HAS_LIBCLANG

#include "worker_thread.h" // Base class: ThreadRequest
#include "clangpch_cache.h"
#include <clang-c/Index.h>
#include <set>

extern const wxEventType wxEVT_CLANG_PCH_CACHE_STARTED ;
extern const wxEventType wxEVT_CLANG_PCH_CACHE_ENDED   ;

enum WorkingContext {
    CTX_CodeCompletion,
    CTX_Calltip,
    CTX_CachePCH,
    CTX_WordCompletion,
	CTX_Macros,
	CTX_ReparseTU
};

struct ClangThreadReply
{
	wxString               filterWord;
	WorkingContext         context;
	wxString               filename;
	wxString               macrosAsString;
	CXCodeCompleteResults *results;
};

class ClangThreadRequest : public ThreadRequest
{
private:
	CXIndex        _index;
	wxString       _fileName;
	wxString       _dirtyBuffer;
	wxString       _compilationArgs;
	wxString       _filterWord;
	WorkingContext _context;
	unsigned       _line;
	unsigned       _column;

public:
	ClangThreadRequest(CXIndex index, const wxString &filename, const wxString &dirtyBuffer, const wxString &compArgs, const wxString &filterWord, WorkingContext context, unsigned line, unsigned column)
		: _index(index)
		, _fileName(filename.c_str())
		, _dirtyBuffer(dirtyBuffer.c_str())
		, _compilationArgs(compArgs.c_str())
		, _filterWord(filterWord)
		, _context(context)
		, _line(line)
		, _column(column) {
	}

	unsigned GetColumn() const {
		return _column;
	}
	const wxString& GetFilterWord() const {
		return _filterWord;
	}
	unsigned GetLine() const {
		return _line;
	}
	virtual ~ClangThreadRequest() {}
	const wxString& GetDirtyBuffer() const {
		return _dirtyBuffer;
	}
	const wxString& GetFileName() const {
		return _fileName;
	}
	const wxString& GetCompilationArgs() const {
		return _compilationArgs;
	}
	CXIndex GetIndex() {
		return _index;
	}
	WorkingContext GetContext() const {
		return _context;
	}
};

////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
class CacheReturner;
class ClangWorkerThread : public WorkerThread
{
	friend class CacheReturner;
protected:
	wxCriticalSection m_cs;
	ClangTUCache     m_cache;

public:
	ClangWorkerThread();
	virtual ~ClangWorkerThread();

protected:
	char**             MakeCommandLine(const wxString& command, int &argc, bool isHeader);
	void               DoCacheResult(CXTranslationUnit TU, const wxString &filename);
	std::set<wxString> DoGetUsedMacros(const wxString &filename);
	
public:
	virtual void ProcessRequest(ThreadRequest* request);
	CXTranslationUnit findEntry(const wxString &filename);
	void              ClearCache();
	bool              IsCacheEmpty();
};

////////////////////////////////////////////////////////////

// A helper class that makes sure that the 
// TU is properly cached when done using
class CacheReturner
{
public:
	ClangWorkerThread* m_thr;
	wxString           m_filename;
	CXTranslationUnit  m_tu;
	
	CacheReturner(ClangWorkerThread* thr, const wxString &filename, CXTranslationUnit TU) 
	: m_thr(thr)
	, m_filename(filename.c_str()) 
	, m_tu(TU)
	{}
	
	~CacheReturner() {
		m_thr->DoCacheResult(m_tu, m_filename.c_str());
	}
};

#endif // HAS_LIBCLANG

#endif // CLANGPCHMAKERTHREAD_H

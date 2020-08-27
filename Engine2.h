#if !defined(LUA_ENGINE2__H)
#define LUA_ENGINE2__H

/*
* Copyright 2019 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the JuaJIT BC Viewer application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

// adopted from NMR Application Framework, see https://github.com/rochus-keller/NAF

#include <QObject>
#include <QSet>
#include <QMap>
#include <QVariant>

typedef struct lua_State lua_State;
typedef struct lua_Debug lua_Debug;

namespace Lua
{
    //* Engine2
	//. Diese Klasse stellt einen Lua-Kontext dar und schirmt gleichzeitig
	//. alle Kunden von Lua ab (nur typedef oben ist sichtbar).
	//. Diese Klasse ist generisch und in keiner Weise mit Spec Assoziiert.

    class Engine2;

    class DbgShell // interface
    {
    public:
        virtual void handleBreak( Engine2*, const QByteArray& source, quint32 line ) = 0;
        virtual void handleAliveSignal(Engine2*) {}
    };

    class Engine2 : public QObject
	{
        Q_OBJECT
    public:
        Engine2(QObject* = 0);
        virtual ~Engine2();

        enum Lib { BASE, REMOVE_LOADS, // Entferne Load-Routinen aus Baselib
            PACKAGE, TABLE, STRING, MATH, OS, IO, LOAD, DBG, BIT, JIT, FFI };
		void addLibrary( Lib );
		void addStdLibs();
        void setPrintToStdout(bool on) { d_printToStdout = on; }

        typedef QSet<quint32> Breaks; // Zeile, zero-based
        typedef QPair<QByteArray, QPair<quint32,QSet<quint32> > > Break; // filename -> prev line, valid line numbers
        typedef QMap<QByteArray,Breaks> BreaksPerScript; // filename -> line numbers

		// Debugging
        void setDbgShell( DbgShell* ds ) { d_dbgShell = ds; }
		void setDebug( bool on );
        void setAliveSignal( bool on );
        bool isDebug() const { return d_debugging; }
        enum DebugCommand { StepInto, StepOver, StepOut, RunToBreakPoint, Abort, AbortSilently };
        void runToNextLine(DebugCommand where = StepInto);
        void runToBreakPoint();
        static int TRAP( lua_State* L );
        static int TRACE( lua_State* L );
        static int ABORT( lua_State* L );
        void setDefaultCmd( DebugCommand c ) { d_defaultDbgCmd = c; }
        DebugCommand getDefaultCmd() const { return d_defaultDbgCmd; }
        void terminate(bool silent = false);
        DebugCommand getCmd() const { return d_dbgCmd; }
        bool isStepping() const { return d_dbgCmd == StepInto || d_dbgCmd == StepOver || d_dbgCmd == StepOut; }
        bool isWaiting() const { return d_waitForCommand; }
        bool isBreakHit() const { return d_breakHit; }
        bool isAborted() const { return d_dbgCmd == AbortSilently || d_dbgCmd == Abort; }
		bool isSilent() const { return d_dbgCmd == AbortSilently; }
        void removeAllBreaks( const QByteArray & = QByteArray() );
        void removeBreak( const QByteArray &, quint32 );
        void addBreak( const QByteArray&, quint32 ); // inkl. : oder @ und one-based
        const Breaks& getBreaks( const QByteArray & ) const;
		const QByteArray& getCurBinary() const { return d_curBinary; }
        struct StackLevel
        {
            quint16 d_level;
            bool d_inC;
            bool d_valid;
            QByteArray d_name;
            QByteArray d_what;
            QByteArray d_source;
            quint32 d_line;
            QSet<quint32> d_lines;
            StackLevel():d_level(0),d_inC(false),d_valid(true),d_line(0){}
        };
        typedef QList<StackLevel> StackLevels;
        StackLevels getStackTrace() const;
        StackLevel getStackLevel(quint16 level, bool withValidLines = true, lua_Debug* ar = 0) const;
        struct LocalVar
        {
            enum Type { NIL, BOOL, NUMBER, STRING, FUNC, TABLE, STRUCT, CDATA, UNKNOWN };
            QByteArray d_name;
            QVariant d_value;
            quint8 d_type;
            bool d_isUv;
            LocalVar():d_isUv(false),d_type(NIL){}
        };
        struct VarAddress
        {
            const void* d_addr;
            quint8 d_type;
            VarAddress(quint8 t = 0, const void* addr = 0):d_type(t),d_addr(addr){}
        };
        typedef QList<LocalVar> LocalVars;
        LocalVars getLocalVars(bool includeUpvals = true, quint8 resolveTableToLevel = 0, int maxArrayIndex = 10) const;

		static Engine2* getInst();
		static void setInst( Engine2* );
		void collect();

        lua_State* getCtx() const { return d_ctx; }
        int getActiveLevel() const { return d_activeLevel; }
        void setActiveLevel(int level );

		// Compile and Execute
        bool executeCmd( const QByteArray& source, const QByteArray& name = QByteArray() );
        bool executeFile( const QByteArray& path );
        bool pushFunction( const QByteArray& source, const QByteArray& name = QByteArray() );
        bool runFunction( int nargs = 0, int nresults = 0 ); // Stack pre: func, par1..parN; post: -
        bool addSourceLib( const QByteArray& source, const QByteArray& libname );
        bool isExecuting() const { return d_running; }
        bool saveBinary( const QByteArray& source, const QByteArray& name, const QByteArray& path );
		static QByteArray getBinaryFromFunc(lua_State *L); // erwartet Func bei -1
        const QByteArrayList& getReturns() const { return d_returns; }


        // Value Support
        QByteArray getTypeName(int arg) const;
        QByteArray __tostring(int arg) const;
        QByteArray getValueString(int arg, bool showAddress = true) const;
        QVariant getValue(int arg, quint8 resolveTableToLevel = 0, int maxArrayIndex = 10) const;
        int pushLocalOrGlobal( const QByteArray& name );
        void pop(int count = 1);

		const QByteArray& getLastError() const { return d_lastError; }
		const char* getVersion() const;

		void error( const char* );
		void print( const char* );

		enum MessageType {
            Print,		// Ausgaben von print(), String mit \n
            Error,		// Ausgaben von _ALERT , String mit \n
            Cout,       // Ausgaben von Stdout, einzelne Zeichen
            Cerr,       // Ausgaben von Stderr, einzelne Zeichen
			LineHit,    // RunToNextLine ist eingetreten
			BreakHit,   // RunToBreakPoint ist auf einen BreakPoint gestossen
            ErrorHit,   // irgendwo im code wurde error() aufgerufen
			BreakPoints,// Breakpoint Zufügen oder Löschen. d_title ist Scriptname
			ActiveLevel,// Der aktive Level wurde verändert
			Started,	// Beginne ein Script laufen zu lassen.
			Continued,  // Fahre nach einem Break weiter
			Finished,  	// Script ist abgelaufen.
			Aborted	    // Script wurde abgebrochen
		};

		class Exception : public std::exception
		{
			QByteArray d_msg;
		public:
			Exception( const char* message ):d_msg(message) {}
			virtual ~Exception() throw() {}
			const char* what() const throw() { return d_msg; }
		};
	signals:
        void onNotify( int messageType, QByteArray val1 = "", int val2 = 0 );
	protected:
		virtual void notify( MessageType messageType, const QByteArray& val1 = "", int val2 = 0 );
    private:
        static StackLevel getStackLevel(lua_State *L, quint16 level, bool withValidLines, lua_Debug* ar);
        static void debugHook(lua_State *L, lua_Debug *ar);
        static void aliveSignal(lua_State *L, lua_Debug *ar);
        static int ErrHandler( lua_State* L );
        void notifyStart();
        void notifyEnd();
        static int _print(lua_State *L);
        static int _writeStdout(lua_State *L);
        static int _writeStderr(lua_State *L);
        static int _writeImp(lua_State *L, bool err);

		BreaksPerScript d_breaks;
        Break d_stepBreak;
        QByteArray d_curScript;
		QByteArray d_curBinary;
        quint32 d_curLine;
		lua_State* d_ctx;
        int d_activeLevel;
		QByteArray d_lastError;
        DebugCommand d_dbgCmd;
        DebugCommand d_defaultDbgCmd;
        DbgShell* d_dbgShell;
        QByteArrayList d_returns;
        quint32 d_aliveCount;
        bool d_breakHit;
        bool d_debugging;
        bool d_aliveSignal;
        bool d_running;
        bool d_waitForCommand;
        bool d_printToStdout;
	};
}

Q_DECLARE_METATYPE(Lua::Engine2::VarAddress)

#endif // !defined(LUA_ENGINE2__H)

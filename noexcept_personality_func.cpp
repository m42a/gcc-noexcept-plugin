// This file is licensed under the GNU General Public License version 3 or
// later, with the additional permissions described in the GCC Runtime Library
// Exception version 3.1.  You should have received both of those licenses
// along with GCC.  If not, see <http://www.gnu.org/licenses/>.

#include <exception>
#include <typeinfo>

#include <cxxabi.h>
#include <unwind.h>

#if defined _WIN32
#define IMPORT __declspec(dllimport)
#else
#define IMPORT __attribute__((visibility("default")))
#endif

#if defined WIN32
#define NOEXPORT
#else
#define NOEXPORT __attribute__((visibility("hidden")))
#endif

typedef unsigned _Unwind_Exception_Class __attribute__((__mode__(__DI__)));

extern "C" IMPORT _Unwind_Reason_Code __gxx_personality_v0(int version, _Unwind_Action actions, _Unwind_Exception_Class exception_class, _Unwind_Exception *ue_header, _Unwind_Context *context) noexcept;

namespace __cxxabiv1
{
	// Copied from unwind-cxx.h
	struct __cxa_exception
	{
		// Manage the exception object itself.
		std::type_info *exceptionType;
		void (_GLIBCXX_CDTOR_CALLABI *exceptionDestructor)(void *);

		// The C++ standard has entertaining rules wrt calling set_terminate
		// and set_unexpected in the middle of the exception cleanup process.
		std::terminate_handler unexpectedHandler;
		std::terminate_handler terminateHandler;

		// The caught exception stack threads through here.
		__cxa_exception *nextException;

		// How many nested handlers have caught this exception.  A negated
		// value is a signal that this object has been rethrown.
		int handlerCount;

#ifdef __ARM_EABI_UNWINDER__
		// Stack of exceptions in cleanups.
		__cxa_exception* nextPropagatingException;

		// The number of active cleanup handlers for this exception.
		int propagationCount;
#else
		// Cache parsed handler data from the personality routine Phase 1
		// for Phase 2 and __cxa_call_unexpected.
		int handlerSwitchValue;
		const unsigned char *actionRecord;
		const unsigned char *languageSpecificData;
		_Unwind_Ptr catchTemp;
		void *adjustedPtr;
#endif

		// The generic exception header.  Must be last.
		_Unwind_Exception unwindHeader;
	};
}

using cxa_ex = __cxxabiv1::__cxa_exception;

struct NOEXPORT __noexcept_marker {};

extern "C" NOEXPORT constexpr const char _ZTS17__noexcept_marker[] = "17__noexcept_marker";
namespace
{
	constexpr __noexcept_marker noexcept_marker_instance;
	struct noexcept_marker_typeinfo : std::type_info
	{
		noexcept_marker_typeinfo() : std::type_info(_ZTS17__noexcept_marker) {}
		~noexcept_marker_typeinfo() override = default;

	protected:
		bool __do_catch(const std::type_info * /*thr_type*/, void **thr_obj, unsigned int) const override
		{
			*thr_obj = (void *)&noexcept_marker_instance;
			return true;
		}
	};

	// Copied from unwind-cxx.h
	// This is the primary exception class we report -- "GNUCC++\0".
	const _Unwind_Exception_Class __gxx_primary_exception_class
	= ((((((((_Unwind_Exception_Class) 'G'
		 << 8 | (_Unwind_Exception_Class) 'N')
		<< 8 | (_Unwind_Exception_Class) 'U')
	       << 8 | (_Unwind_Exception_Class) 'C')
	      << 8 | (_Unwind_Exception_Class) 'C')
	     << 8 | (_Unwind_Exception_Class) '+')
	    << 8 | (_Unwind_Exception_Class) '+')
	   << 8 | (_Unwind_Exception_Class) '\0');

	// This is the dependent (from std::rethrow_exception) exception class we report
	// "GNUCC++\x01"
	const _Unwind_Exception_Class __gxx_dependent_exception_class
	= ((((((((_Unwind_Exception_Class) 'G'
		 << 8 | (_Unwind_Exception_Class) 'N')
		<< 8 | (_Unwind_Exception_Class) 'U')
	       << 8 | (_Unwind_Exception_Class) 'C')
	      << 8 | (_Unwind_Exception_Class) 'C')
	     << 8 | (_Unwind_Exception_Class) '+')
	    << 8 | (_Unwind_Exception_Class) '+')
	   << 8 | (_Unwind_Exception_Class) '\x01');

	void *get_adjusted_ptr(_Unwind_Exception_Class exception_class, _Unwind_Exception *header)
	{
		if (exception_class == __gxx_primary_exception_class || exception_class == __gxx_dependent_exception_class)
			return ((cxa_ex *)(header + 1))[-1].adjustedPtr;
		return nullptr;
	}
}

extern "C" NOEXPORT noexcept_marker_typeinfo _ZTI17__noexcept_marker;
noexcept_marker_typeinfo _ZTI17__noexcept_marker;

extern "C" NOEXPORT _Unwind_Reason_Code __noexcept_personality(int version, _Unwind_Action actions, _Unwind_Exception_Class exception_class, _Unwind_Exception *ue_header, _Unwind_Context *context) noexcept
{
	auto base_ret = __gxx_personality_v0(version, actions, exception_class, ue_header, context);
	if (version == 1 && actions == _UA_SEARCH_PHASE && base_ret == _URC_HANDLER_FOUND)
	{
		auto adj_ptr = get_adjusted_ptr(exception_class, ue_header);
		if (adj_ptr == &noexcept_marker_instance)
		{
			__cxxabiv1::__cxa_begin_catch(ue_header);
			// We don't need to call the exception's terminateHandler member because unwinding hasn't happened yet, so this thread cannot have changed the handler or synchronized with another thread that changed the handler.
			std::terminate();
		}
	}
	return base_ret;
}

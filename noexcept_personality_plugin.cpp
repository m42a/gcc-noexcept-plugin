// This file is licensed under the GNU General Public License version 3 or
// later.  You should have received this license along with GCC.  If not, see
// <http://www.gnu.org/licenses/>.

#include <gcc-plugin.h>
#include "plugin-version.h"

#include <cp/cp-tree.h>
#include <gimple.h>
#include <context.h>
#include <except.h>
#include <gimple-expr.h>
#include <gimple-iterator.h>
#include <langhooks.h>
#include <options.h>
#include <rtl.h>
#include <stringpool.h>
#include <tree-check.h>
#include <tree-pass.h>
#include <tree.h>
#include <varasm.h>

int plugin_is_GPL_compatible;

namespace
{
	const pass_data noexcept_personality_pass_data{
		GIMPLE_PASS,
		"noexcept_personality",
		OPTGROUP_NONE,
		TV_TREE_EH,
		PROP_gimple_lcf,
		0,
		0,
		0,
		0,
	};

	class noexcept_personality_pass : public gimple_opt_pass
	{
		noexcept_personality_pass(gcc::context *g) : gimple_opt_pass(noexcept_personality_pass_data, g) {}

		// We can't create the personality separately for every function because that prevents inlining, which will fail the build if a function is marked always_inline.  I'm not familiar with the GCC garbage collector, so I don't know how to make sure this isn't collected between execute invocations.
		tree personality = nullptr;
	public:
		static noexcept_personality_pass *make()
		{
			return new noexcept_personality_pass(g);
		}

		unsigned int execute(function *fun) final override
		{
			auto decl_type = TREE_TYPE(fun->decl);
			bool is_noexcept = TYPE_NOEXCEPT_P(decl_type);

			if (!personality)
			{
				personality = [&]{
					// Copied from build_personality_function (in gcc/expr.cc)
					auto type = build_function_type_list (unsigned_type_node,
							integer_type_node, integer_type_node,
							long_long_unsigned_type_node,
							ptr_type_node, ptr_type_node, NULL_TREE);
					auto decl = build_decl (UNKNOWN_LOCATION, FUNCTION_DECL,
							get_identifier ("__noexcept_personality"), type);
					DECL_ARTIFICIAL (decl) = 1;
					DECL_EXTERNAL (decl) = 1;
					TREE_PUBLIC (decl) = 1;

					SET_SYMBOL_REF_DECL (XEXP (DECL_RTL (decl), 0), NULL);
					return decl;
				}();
			}
			// function_needs_eh_personality doesn't work until after the eh pass runs, so set the personality unconditionally
			DECL_FUNCTION_PERSONALITY (fun->decl) = personality;

			if (is_noexcept)
			{
				auto body = gimple_body(fun->decl);

				for (gimple_stmt_iterator gsi = gsi_start(body); !gsi_end_p(gsi); gsi_next(&gsi))
				{
					auto stmt = gsi_stmt(gsi);
					if (gimple_code(stmt) != GIMPLE_TRY)
						continue;
					if (gimple_try_kind(stmt) != GIMPLE_TRY_CATCH)
						continue;
					auto catch_block = gimple_try_cleanup(stmt);
					auto catch_gsi = gsi_start(catch_block);
					auto catch_stmt = gsi_stmt(catch_gsi);
					if (gimple_code(catch_stmt) != GIMPLE_EH_MUST_NOT_THROW)
						continue;
					auto noexcept_marker_decl = [&]{
						auto ref = make_node(RECORD_TYPE);
						auto type_decl = build_decl(UNKNOWN_LOCATION, TYPE_DECL, get_identifier("__noexcept_marker"), ref);
						TYPE_NAME(ref) = type_decl;
						TYPE_STUB_DECL(ref) = type_decl;
						return ref;
					}();
					auto call_terminate = [&]{
						gimple_seq body(nullptr);

						auto get_eh_ptr_decl = builtin_decl_explicit(BUILT_IN_EH_POINTER);
						auto get_eh_ptr_call = gimple_build_call(get_eh_ptr_decl, 1, integer_zero_node);
						auto eh_ptr_var = create_tmp_var(ptr_type_node, "eh_ptr");
						gimple_call_set_lhs(get_eh_ptr_call, eh_ptr_var);
						gimple_seq_add_stmt(&body, get_eh_ptr_call);

						// This should call __cxa_call_terminate to use the terminate handler in the exception, but that's not exported on linux, and we should never get here anyway so it doesn't really matter
						auto begin_catch_type = build_function_type_list(ptr_type_node, ptr_type_node, NULL_TREE);
						auto begin_catch_decl = build_decl(UNKNOWN_LOCATION, FUNCTION_DECL, get_identifier("__cxa_begin_catch"), begin_catch_type);
						DECL_ARTIFICIAL(begin_catch_decl) = 1;
						DECL_EXTERNAL(begin_catch_decl) = 1;
						TREE_PUBLIC(begin_catch_decl) = 1;
						auto begin_catch_call = gimple_build_call(begin_catch_decl, 1, eh_ptr_var);
						gimple_seq_add_stmt(&body, begin_catch_call);

						auto terminate_decl = gimple_eh_must_not_throw_fndecl(as_a<geh_mnt *>(catch_stmt));
						auto terminate_call = gimple_build_call(terminate_decl, 0);
						gimple_seq_add_stmt(&body, terminate_call);
						return body;
					}();
					auto new_body = gimple_build_catch(noexcept_marker_decl, call_terminate);
					gimple_try_set_cleanup(as_a<gtry *>(stmt), new_body);
					break;
				}
			}
			return 0;
		}
	};
}

int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *ver)
{
	if (!plugin_default_version_check(ver, &gcc_version))
		return 1;

	static struct plugin_info noexcept_personality_info = { .version = "0.1", .help = "Mark noexcept functions with a personality that doesn't unwind"};

	register_callback(plugin_info->base_name, PLUGIN_INFO, nullptr, &noexcept_personality_info);

	register_pass_info noexcept_pass{
		.pass = noexcept_personality_pass::make(),
		.reference_pass_name = "eh",
		.ref_pass_instance_number = 1,
		.pos_op = PASS_POS_INSERT_BEFORE,
	};
	register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr, &noexcept_pass);
	return 0;
}

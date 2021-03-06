/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/chain/chain_config.hpp>
#include <eosio/chain/authority_checker.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/asset.hpp>
#include <eosio/testing/tester.hpp>

#include <eosio/utilities/key_conversion.hpp>
#include <eosio/utilities/rand.hpp>

#include <fc/io/json.hpp>

#include <boost/test/unit_test.hpp>

#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain/wasm_interface_private.hpp>


#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace eosio::chain;
namespace eosio
{
using namespace chain;
using namespace std;

BOOST_AUTO_TEST_SUITE(misc_tests)

/// Test processing of unbalanced strings
BOOST_AUTO_TEST_CASE(json_from_string_test)
{
  bool exc_found = false;
  try {
    auto val = fc::json::from_string("{\"}");
  } catch(...) {
    exc_found = true;
  }
  BOOST_CHECK_EQUAL(exc_found, true);

  exc_found = false;
  try {
    auto val = fc::json::from_string("{\"block_num_or_id\":5");
  } catch(...) {
    exc_found = true;
  }
  BOOST_CHECK_EQUAL(exc_found, true);
}

/// Test that our deterministic random shuffle algorithm gives the same results in all environments
BOOST_AUTO_TEST_CASE(deterministic_randomness)
{ try {
   utilities::rand::random rng(123454321);
   vector<string> words = {"infamy", "invests", "estimated", "potters", "memorizes", "hal9000"};
   rng.shuffle(words);
   BOOST_TEST(fc::json::to_string(words) ==
                     fc::json::to_string(vector<string>{"hal9000","infamy","invests","estimated","memorizes","potters"}));
   rng.shuffle(words);
   BOOST_TEST(fc::json::to_string(words) ==
                     fc::json::to_string(vector<string>{"memorizes","infamy","hal9000","potters","estimated","invests"}));
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(deterministic_distributions)
{ try {
   utilities::rand::random rng(123454321);

   BOOST_TEST(rng.next() == UINT64_C(13636622732572118961));
   BOOST_TEST(rng.next() == UINT64_C(8049736256506128729));
   BOOST_TEST(rng.next() ==  UINT64_C(1224405983932261174));

   std::vector<int> nums = {0, 1, 2};

   rng.shuffle(nums);
   std::vector<int> a{2, 0, 1};
   BOOST_TEST(std::equal(nums.begin(), nums.end(), a.begin()));
   rng.shuffle(nums);
   std::vector<int> b{0, 2, 1};
   BOOST_TEST(std::equal(nums.begin(), nums.end(), b.begin()));
   rng.shuffle(nums);
   std::vector<int> c{1, 0, 2};
   BOOST_TEST(std::equal(nums.begin(), nums.end(), c.begin()));
} FC_LOG_AND_RETHROW() }

struct permission_visitor {
   std::vector<permission_level> permissions;
   std::vector<size_t> size_stack;
   bool _log;

   permission_visitor(bool log = false) : _log(log) {}

   void operator()(const permission_level& permission) {
      permissions.push_back(permission);
   }

   void push_undo() {
      if( _log )
         ilog("push_undo called");
      size_stack.push_back(permissions.size());
   }

   void pop_undo() {
      if( _log )
         ilog("pop_undo called");
      FC_ASSERT( size_stack.back() <= permissions.size() && size_stack.size() >= 1,
                 "invariant failure in test permission_visitor" );
      permissions.erase( permissions.begin() + size_stack.back(), permissions.end() );
      size_stack.pop_back();
   }

   void squash_undo() {
      if( _log )
         ilog("squash_undo called");
      FC_ASSERT( size_stack.size() >= 1, "invariant failure in test permission_visitor" );
      size_stack.pop_back();
   }

};

BOOST_AUTO_TEST_CASE(authority_checker)
{ try {
   testing::TESTER test;
   auto a = test.get_public_key("a", "active");
   auto b = test.get_public_key("b", "active");
   auto c = test.get_public_key("c", "active");

   auto GetNullAuthority = [](auto){abort(); return authority();};

   permission_visitor pv;
   auto A = authority(2, {key_weight{a, 1}, key_weight{b, 1}});
   {
      auto checker = make_auth_checker(GetNullAuthority, pv, 2, {a, b});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 2);
      BOOST_TEST(checker.unused_keys().size() == 0);
   }
   {
      auto checker = make_auth_checker(GetNullAuthority, pv, 2, {a, c});
      BOOST_TEST(!checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 0);
      BOOST_TEST(checker.unused_keys().size() == 2);
   }
   {
      auto checker = make_auth_checker(GetNullAuthority, pv, 2, {a, b, c});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 2);
      BOOST_TEST(checker.used_keys().count(a) == 1);
      BOOST_TEST(checker.used_keys().count(b) == 1);
      BOOST_TEST(checker.unused_keys().size() == 1);
      BOOST_TEST(checker.unused_keys().count(c) == 1);
   }
   {
      auto checker = make_auth_checker(GetNullAuthority, pv, 2, {b, c});
      BOOST_TEST(!checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 0);
   }

   A = authority(3, {key_weight{a, 1}, key_weight{b, 1}, key_weight{c, 1}});
   BOOST_TEST(make_auth_checker(GetNullAuthority, pv, 2, {c, b, a}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetNullAuthority, pv, 2, {a, b}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetNullAuthority, pv, 2, {a, c}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetNullAuthority, pv, 2, {b, c}).satisfied(A));

   A = authority(1, {key_weight{a, 1}, key_weight{b, 1}});
   BOOST_TEST(make_auth_checker(GetNullAuthority, pv, 2, {a}).satisfied(A));
   BOOST_TEST(make_auth_checker(GetNullAuthority, pv, 2, {b}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetNullAuthority, pv, 2, {c}).satisfied(A));

   A = authority(1, {key_weight{a, 2}, key_weight{b, 1}});
   BOOST_TEST(make_auth_checker(GetNullAuthority, pv, 2, {a}).satisfied(A));
   BOOST_TEST(make_auth_checker(GetNullAuthority, pv, 2, {b}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetNullAuthority, pv, 2, {c}).satisfied(A));

   auto GetCAuthority = [c](auto){
      return authority(1, {key_weight{c, 1}});
   };

   A = authority(2, {key_weight{a, 2}, key_weight{b, 1}}, {permission_level_weight{{"hello",  "world"}, 1}});
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {a});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(checker.all_keys_used());
   }
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {b});
      BOOST_TEST(!checker.satisfied(A));
      BOOST_TEST(checker.used_keys().size() == 0);
      BOOST_TEST(checker.unused_keys().size() == 1);
      BOOST_TEST(checker.unused_keys().count(b) == 1);
   }
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {c});
      BOOST_TEST(!checker.satisfied(A));
      BOOST_TEST(checker.used_keys().size() == 0);
      BOOST_TEST(checker.unused_keys().size() == 1);
      BOOST_TEST(checker.unused_keys().count(c) == 1);
   }
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {b, c});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 2);
      BOOST_TEST(checker.unused_keys().size() == 0);
      BOOST_TEST(checker.used_keys().count(b) == 1);
      BOOST_TEST(checker.used_keys().count(c) == 1);
   }
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {b, c, a});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 1);
      BOOST_TEST(checker.used_keys().count(a) == 1);
      BOOST_TEST(checker.unused_keys().size() == 2);
      BOOST_TEST(checker.unused_keys().count(b) == 1);
      BOOST_TEST(checker.unused_keys().count(c) == 1);
   }

   A = authority(3, {key_weight{a, 2}, key_weight{b, 1}}, {permission_level_weight{{"hello",  "world"}, 3}});
   pv._log = true;
   {
      pv.permissions.clear();
      pv.size_stack.clear();
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {a, b});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(checker.all_keys_used());
      BOOST_TEST(pv.permissions.size() == 0);
   }
   {
      pv.permissions.clear();
      pv.size_stack.clear();
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {a, b, c});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 1);
      BOOST_TEST(checker.used_keys().count(c) == 1);
      BOOST_TEST(checker.unused_keys().size() == 2);
      BOOST_TEST(checker.unused_keys().count(a) == 1);
      BOOST_TEST(checker.unused_keys().count(b) == 1);
      BOOST_TEST(pv.permissions.size() == 1);
      BOOST_TEST(pv.permissions.back().actor == "hello");
      BOOST_TEST(pv.permissions.back().permission == "world");
   }
   pv._log = false;

   A = authority(2, {key_weight{a, 1}, key_weight{b, 1}}, {permission_level_weight{{"hello",  "world"}, 1}});
   BOOST_TEST(!make_auth_checker(GetCAuthority, pv, 2, {a}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetCAuthority, pv, 2, {b}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetCAuthority, pv, 2, {c}).satisfied(A));
   BOOST_TEST(make_auth_checker(GetCAuthority, pv, 2, {a, b}).satisfied(A));
   BOOST_TEST(make_auth_checker(GetCAuthority, pv, 2, {b, c}).satisfied(A));
   BOOST_TEST(make_auth_checker(GetCAuthority, pv, 2, {a, c}).satisfied(A));
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {a, b, c});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 2);
      BOOST_TEST(checker.unused_keys().size() == 1);
      BOOST_TEST(checker.unused_keys().count(c) == 1);
   }

   A = authority(2, {key_weight{a, 1}, key_weight{b, 1}}, {permission_level_weight{{"hello",  "world"}, 2}});
   BOOST_TEST(make_auth_checker(GetCAuthority, pv, 2, {a, b}).satisfied(A));
   BOOST_TEST(make_auth_checker(GetCAuthority, pv, 2, {c}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetCAuthority, pv, 2, {a}).satisfied(A));
   BOOST_TEST(!make_auth_checker(GetCAuthority, pv, 2, {b}).satisfied(A));
   {
      auto checker = make_auth_checker(GetCAuthority, pv, 2, {a, b, c});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 1);
      BOOST_TEST(checker.unused_keys().size() == 2);
      BOOST_TEST(checker.used_keys().count(c) == 1);
   }

   auto d = test.get_public_key("d", "active");
   auto e = test.get_public_key("e", "active");

   auto GetAuthority = [d, e] (const permission_level& perm) {
      if (perm.actor == "top")
         return authority(2, {key_weight{d, 1}}, {permission_level_weight{{"bottom",  "bottom"}, 1}});
      return authority{1, {{e, 1}}, {}};
   };

   A = authority(5, {key_weight{a, 2}, key_weight{b, 2}, key_weight{c, 2}}, {permission_level_weight{{"top",  "top"}, 5}});
   {
      auto checker = make_auth_checker(GetAuthority, pv, 2, {d, e});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(checker.all_keys_used());
   }
   {
      auto checker = make_auth_checker(GetAuthority, pv, 2, {a, b, c, d, e});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 2);
      BOOST_TEST(checker.unused_keys().size() == 3);
      BOOST_TEST(checker.used_keys().count(d) == 1);
      BOOST_TEST(checker.used_keys().count(e) == 1);
   }
   {
      auto checker = make_auth_checker(GetAuthority, pv, 2, {a, b, c, e});
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.used_keys().size() == 3);
      BOOST_TEST(checker.unused_keys().size() == 1);
      BOOST_TEST(checker.used_keys().count(a) == 1);
      BOOST_TEST(checker.used_keys().count(b) == 1);
      BOOST_TEST(checker.used_keys().count(c) == 1);
   }
   BOOST_TEST(make_auth_checker(GetAuthority, pv, 1, {a, b, c}).satisfied(A));
   // Fails due to short recursion depth limit
   BOOST_TEST(!make_auth_checker(GetAuthority, pv, 1, {d, e}).satisfied(A));

   BOOST_TEST(b < a);
   BOOST_TEST(b < c);
   BOOST_TEST(a < c);
   {
      // valid key order: c > a > b
      A = authority(2, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}});
      // valid key order: c > b
      auto B = authority(1, {key_weight{c, 1}, key_weight{b, 1}});
      // invalid key order: b < c
      auto C = authority(1, {key_weight{c, 1}, key_weight{b, 1}, key_weight{c, 1}});
      // invalid key order: duplicate c
      auto D = authority(1, {key_weight{c, 1}, key_weight{c, 1}, key_weight{b, 1}});
      // invalid key order: duplicate b
      auto E = authority(1, {key_weight{c, 1}, key_weight{b, 1}, key_weight{b, 1}});
      // unvalid: insufficient weight
      auto F = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}});

      auto checker = make_auth_checker(GetNullAuthority, pv, 2, {a, b, c});
      BOOST_TEST(validate(A));
      BOOST_TEST(validate(B));
      BOOST_TEST(!validate(C));
      BOOST_TEST(!validate(D));
      BOOST_TEST(!validate(E));
      BOOST_TEST(!validate(F));

      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.unused_keys().count(c) == 1);
      BOOST_TEST(checker.unused_keys().count(a) == 1);
      BOOST_TEST(checker.unused_keys().count(b) == 1);
      BOOST_TEST(checker.satisfied(A));
      BOOST_TEST(checker.satisfied(B));
      BOOST_TEST(!checker.all_keys_used());
      BOOST_TEST(checker.unused_keys().count(c) == 0);
      BOOST_TEST(checker.unused_keys().count(a) == 0);
      BOOST_TEST(checker.unused_keys().count(b) == 1);
   }
   {
      auto A2 = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hi",  "world"}, 1},
                                permission_level_weight{{"hello",  "world"}, 1},
                                   permission_level_weight{{"a",  "world"}, 1}
                          });
      auto B2 = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hello",  "world"}, 1}
                          });
      auto C2 = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hello",  "world"}, 1},
                                permission_level_weight{{"hello",  "there"}, 1}
                          });
      // invalid: duplicate
      auto D2 = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hello",  "world"}, 1},
                                permission_level_weight{{"hello",  "world"}, 2}
                          });
      // invalid: wrong order
      auto E2 = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hello",  "there"}, 1},
                                permission_level_weight{{"hello",  "world"}, 2}
                          });
      // invalid: wrong order
      auto F2 = authority(4, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hello",  "world"}, 1},
                                permission_level_weight{{"hi",  "world"}, 2}
                          });

      // invalid: insufficient weight
      auto G2 = authority(7, {key_weight{c, 1}, key_weight{a, 1}, key_weight{b, 1}},
                          {permission_level_weight{{"hi",  "world"}, 1},
                                permission_level_weight{{"hello",  "world"}, 1},
                                   permission_level_weight{{"a",  "world"}, 1}
                          });

      BOOST_TEST(validate(A2));
      BOOST_TEST(validate(B2));
      BOOST_TEST(validate(C2));
      BOOST_TEST(!validate(D2));
      BOOST_TEST(!validate(E2));
      BOOST_TEST(!validate(F2));
      BOOST_TEST(!validate(G2));
   }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(alphabetic_sort)
{ try {

  vector<string> words = {
    "com.o",
    "te",
    "a.....5",
    "a...4",
    ".va.ku",
    "gh",
    "1ho.la",
    "g1",
    "g",
    "a....2",
    "gg",
    "va",
    "lale.....12b",
    "a....3",
    "a....1",
    "..g",
    ".g",
    "....g",
    "a....y",
    "...g",
    "lale.....333",
  };

  std::sort(words.begin(), words.end(), std::less<string>());

  vector<uint64_t> uwords;
  for(const auto w: words) {
    auto n = name(w.c_str());
    uwords.push_back(n.value);
  }

  std::sort(uwords.begin(), uwords.end(), std::less<uint64_t>());

  vector<string> tmp;
  for(const auto uw: uwords) {
    auto str = name(uw).to_string();
    tmp.push_back(str);
  }

  for(int i = 0; i < words.size(); ++i ) {
    BOOST_TEST(tmp[i] == words[i]);
  }

} FC_LOG_AND_RETHROW() }

template <typename T> using shared_alloc  = bip::allocator<T,bip::managed_mapped_file::segment_manager>;
template <typename K, typename V, typename P = std::pair<K,V>, typename Cmp = std::less<K> >
                      using shared_map    = boost::container::flat_map<K, V, Cmp, shared_alloc<P> >;

using dataset_t     = shared_map<shared_string, shared_vector<char> >;


BOOST_AUTO_TEST_CASE(instantiation_cache_mem_leak) try {

   bip::managed_mapped_file seg(bip::open_or_create,"./test.db", 1024*1024);

   auto& db = *seg.find_or_construct<dataset_t>
       ("DATA")
       (
        std::less<shared_string>(),
        dataset_t::allocator_type(seg.get_segment_manager())
       );

    auto alloc = db.get_allocator().get_segment_manager();

   int count = 0;

   std::string wast_code = R"=====(
(module
(import "env" "require_auth" (func $require_auth (param i64)))
(import "env" "eosio_assert" (func $eosio_assert (param i32 i32)))
   (table 0 anyfunc)
   (memory $0 1)
   (export "apply" (func $apply))
   (func $i64_trunc_u_f64 (param $0 f64) (result i64) (i64.trunc_u/f64 (get_local $0)))
   (func $test (param $0 i64))
   (func $apply (param $0 i64)(param $1 i64)(param $2 i64)
   )=====";

   std::string code = wast_code;
   code += "(call $test (call $i64_trunc_u_f64 (f64.const 1)))\n";
   code += "))";

   wasm_interface_impl wasm_imp(wasm_interface::vm_type::binaryen);

   auto wasm = ::eosio::chain::wast_to_wasm(code);

   auto wasm_code = shared_vector<char>(alloc);
   wasm_code.resize(wasm.size());
   memcpy(wasm_code.data(), wasm.data(), wasm.size());

   auto code_id = fc::sha256::hash( wasm_code.data(), (uint32_t)wasm_code.size() );

   wasm_imp.get_instantiated_module(N(test1), code_id, wasm_code);
   BOOST_REQUIRE_EQUAL(1, wasm_imp.instantiation_cache.size());

   wasm_imp.get_instantiated_module(N(test2), code_id, wasm_code);
   BOOST_REQUIRE_EQUAL(1, wasm_imp.instantiation_cache.size());

   code = wast_code;
   code += "(call $test (call $i64_trunc_u_f64 (f64.const 1)))\n";
   code += "(call $test (call $i64_trunc_u_f64 (f64.const 1)))\n";
   code += "))";
   wasm = ::eosio::chain::wast_to_wasm(code);

   wasm_code = shared_vector<char>(alloc);
   wasm_code.resize(wasm.size());
   memcpy(wasm_code.data(), wasm.data(), wasm.size());

   code_id = fc::sha256::hash( wasm_code.data(), (uint32_t)wasm_code.size() );
   wasm_imp.get_instantiated_module(N(test1), code_id, wasm_code);
   BOOST_REQUIRE_EQUAL(2, wasm_imp.instantiation_cache.size());

   wasm_imp.get_instantiated_module(N(test2), code_id, wasm_code);
   BOOST_REQUIRE_EQUAL(1, wasm_imp.instantiation_cache.size());

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()

} // namespace eosio

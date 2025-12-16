add_test([=[TupleTest.TableHeapTest]=]  /Users/cscs/Downloads/bustub_initial/build/test/tuple_test [==[--gtest_filter=TupleTest.DISABLED_TableHeapTest]==] --gtest_also_run_disabled_tests [==[--gtest_color=auto]==] [==[--gtest_output=xml:/Users/cscs/Downloads/bustub_initial/build/test/tuple_test.xml]==] [==[--gtest_catch_exceptions=0]==])
set_tests_properties([=[TupleTest.TableHeapTest]=]  PROPERTIES DISABLED TRUE WORKING_DIRECTORY /Users/cscs/Downloads/bustub_initial/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==] TIMEOUT 120)
set(  tuple_test_TESTS TupleTest.TableHeapTest)

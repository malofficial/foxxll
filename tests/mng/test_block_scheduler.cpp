/***************************************************************************
 *  tests/mng/test_block_scheduler.cpp
 *
 *  Part of FOXXLL. See http://foxxll.org
 *
 *  Copyright (C) 2010-2011 Raoul Steffen <R-Steffen@gmx.de>
 *  Copyright (C) 2013 Timo Bingmann <tb@panthema.net>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#include <iostream>
#include <limits>

#include <tlx/cmdline_parser.hpp>
#include <tlx/die.hpp>
#include <tlx/logger.hpp>

#include <foxxll/mng/block_scheduler.hpp>

using foxxll::external_size_type;

// forced instantiation
template class foxxll::block_scheduler<foxxll::swappable_block<size_t, 1024> >;

template <class IBT>
void set_pattern_A(IBT& ib)
{
    for (size_t i = 0; i < ib.size; ++i)
        ib[i] = i;
}

template <class IBT>
size_t test_pattern_A(IBT& ib)
{
    size_t num_err = 0;
    for (size_t i = 0; i < ib.size; ++i)
        num_err += (ib[i] != i);
    return num_err;
}

template <class IBT>
void set_pattern_B(IBT& ib)
{
    for (size_t i = 0; i < ib.size; ++i)
        ib[i] = ib.size - i;
}

template <class IBT>
size_t test_pattern_B(IBT& ib)
{
    size_t num_err = 0;
    for (size_t i = 0; i < ib.size; ++i)
        num_err += (ib[i] != ib.size - i);
    return num_err;
}

const size_t block_size = 1024;
using value_type = size_t;

size_t internal_memory = 0;

using block_scheduler_type = foxxll::block_scheduler<foxxll::swappable_block<value_type, block_size> >;
using swappable_block_type = foxxll::swappable_block<value_type, block_size>;
using swappable_block_identifier_type = block_scheduler_type::swappable_block_identifier_type;
using internal_block_type = block_scheduler_type::internal_block_type;
using external_block_type = block_scheduler_type::external_block_type;

void test1()
{
    // ------------------- call all functions -----------------------
    LOG1 << "first test: call all functions";

    // prepare an external_block with pattern A
    external_block_type ext_bl;
    foxxll::block_manager::get_instance()->new_block(foxxll::striping(), ext_bl);
    internal_block_type* int_bl = new internal_block_type;
    set_pattern_A(*int_bl);
    int_bl->write(ext_bl)->wait();

    // the block_scheduler may use internal_memory byte for caching
    block_scheduler_type* bs_ptr = new block_scheduler_type(internal_memory);
    block_scheduler_type& bs = *bs_ptr;
    // make sure is not just recording a prediction sequence
    die_unless(! bs.is_simulating());

    // allocate a swappable_block and store its identifier
    swappable_block_identifier_type sbi1 = bs.allocate_swappable_block();
    die_verbose_unless(!bs.is_initialized(sbi1), "new block should not be initialized?");

    // initialize the swappable_block with the prepared external_block
    bs.initialize(sbi1, ext_bl);
    {
        // acquire the swappable_block to gain access
        internal_block_type& ib = bs.acquire(sbi1);

        // read from the swappable_block. it should contain pattern A
        size_t num_err = 0;
        for (size_t i = 0; i < block_size; ++i)
            num_err += (ib[i] != i);
        die_verbose_unless(
            num_err == 0,
            "previously initialized block had " << num_err << " errors."
        );
    }
    {
        // get a new reference to the already allocated block (because we forgot the old one)
        internal_block_type& ib = bs.get_internal_block(sbi1);

        // write pattern B
        for (size_t i = 0; i < block_size; ++i)
            ib[i] = block_size - i;

        // release the swappable_block. changes have to be stored. it may now be swapped out.
        bs.release(sbi1, true);
    }

    // allocate a second swappable_block and store its identifier
    swappable_block_identifier_type sbi2 = bs.allocate_swappable_block();
    die_verbose_unless(!bs.is_initialized(sbi2), "new block should not be initialized?");

    {
        // acquire the swappable_block to gain access
        internal_block_type& ib1 = bs.acquire(sbi1);
        // acquire the swappable_block to gain access because it was uninitialized, it now becomes initialized
        internal_block_type& ib2 = bs.acquire(sbi2);

        // copy pattern B
        for (size_t i = 0; i < block_size; ++i)
            ib2[i] = ib1[i];

        // release the swappable_block. no changes happened.
        bs.release(sbi1, false);
        bs.release(sbi2, true);
    }

    // both blocks should now be initialized
    die_verbose_unless(bs.is_initialized(sbi1), "block should not be initialized!");
    die_verbose_unless(bs.is_initialized(sbi2), "block should not be initialized!");

    // get the external_block
    ext_bl = bs.extract_external_block(sbi2);
    die_verbose_unless(!bs.is_initialized(sbi2), "block should not be initialized after extraction!");

    // free external block 1
    bs.deinitialize(sbi1);
    die_verbose_unless(!bs.is_initialized(sbi1), "block should not be initialized after deinitialize!");

    // free the swappable_blocks
    bs.free_swappable_block(sbi1);
    bs.free_swappable_block(sbi2);
    bs.explicit_timestep();

    // switch to simulation mode
    foxxll::block_scheduler_algorithm_simulation<swappable_block_type>* asim =
        new foxxll::block_scheduler_algorithm_simulation<swappable_block_type>(bs);
    delete bs.switch_algorithm_to(asim);

    // allocate swappable block
    swappable_block_identifier_type sbi = bs.allocate_swappable_block();

    // perform acquire and release sequence
    bs.acquire(sbi);
    bs.acquire(sbi);
    bs.release(sbi, true);
    bs.explicit_timestep();
    bs.release(sbi, false);
    bs.deinitialize(sbi);
    bs.initialize(sbi, external_block_type());
    if (bs.is_simulating())
        bs.extract_external_block(sbi);
    else
        bs.extract_external_block(sbi);
    bs.free_swappable_block(sbi);

    // output prediction sequence
    if (true)
    {
        block_scheduler_type::prediction_sequence_type ps = bs.get_prediction_sequence();
        for (block_scheduler_type::prediction_sequence_type::iterator it = ps.begin(); it != ps.end(); ++it)
            LOG1 << "id: " << it->id << " op: " << it->op << " t: " << it->time;
    }

    // switch to LFD processing
    delete bs.switch_algorithm_to(
        new foxxll::block_scheduler_algorithm_offline_lfd<swappable_block_type>(asim)
    );

    sbi = bs.allocate_swappable_block();
    bs.acquire(sbi);
    bs.acquire(sbi);
    bs.release(sbi, true);
    bs.explicit_timestep();
    bs.release(sbi, false);
    bs.deinitialize(sbi);
    bs.initialize(sbi, external_block_type());
    bs.extract_external_block(sbi);
    bs.free_swappable_block(sbi);

#if 0
    // 2013-tb: segfaults due to missing prediction sequence? TODO
    delete bs.switch_algorithm_to(
        new foxxll::block_scheduler_algorithm_offline_lru_prefetching<swappable_block_type>(asim)
    );
    sbi = bs.allocate_swappable_block();
    bs.acquire(sbi);
    bs.acquire(sbi);
    bs.release(sbi, true);
    bs.explicit_timestep();
    bs.release(sbi, false);
    bs.deinitialize(sbi);
    bs.initialize(sbi, external_block_type());
    bs.extract_external_block(sbi);
    bs.free_swappable_block(sbi);
#endif

    delete bs_ptr;

    int_bl->read(ext_bl)->wait();
    die_verbose_unless(
        test_pattern_B(*int_bl) == 0,
        "after extraction changed block should contain pattern B."
    );
    delete int_bl;
}

void test2()
{
    // ---------- force swapping ---------------------
    LOG1 << "next test: force swapping";
    constexpr size_t kNumSB = 5;

    // only 3 internal_blocks allowed
    block_scheduler_type* bs_ptr = new block_scheduler_type(block_size * sizeof(value_type) * 3);
    block_scheduler_type& bs = *bs_ptr;

    // allocate blocks
    swappable_block_identifier_type sbi[kNumSB];
    for (size_t i = 0; i < kNumSB; ++i)
        sbi[i] = bs.allocate_swappable_block();

    // fill 3 blocks
    internal_block_type* ib[kNumSB];
    ib[0] = &bs.acquire(sbi[0]);
    ib[1] = &bs.acquire(sbi[1]);
    ib[2] = &bs.acquire(sbi[2]);
    set_pattern_A(*ib[0]);
    set_pattern_A(*ib[1]);
    set_pattern_A(*ib[2]);
    bs.release(sbi[0], true);
    bs.release(sbi[1], true);
    bs.release(sbi[2], true);

    // fill 2 blocks, now some others have to be swapped out
    ib[3] = &bs.acquire(sbi[3]);
    ib[4] = &bs.acquire(sbi[4]);
    set_pattern_A(*ib[3]);
    set_pattern_A(*ib[4]);
    bs.release(sbi[3], true);
    bs.release(sbi[4], true);

    ib[2] = &bs.acquire(sbi[2]); // this block can still be cached
    ib[3] = &bs.acquire(sbi[3]); // as can this
    ib[1] = &bs.acquire(sbi[1]); // but not this
    die_verbose_unless(test_pattern_A(*ib[1]) == 0, "Block 1 had errors.");
    die_verbose_unless(test_pattern_A(*ib[2]) == 0, "Block 2 had errors.");
    die_verbose_unless(test_pattern_A(*ib[3]) == 0, "Block 3 had errors.");

    bs.release(sbi[1], false);
    bs.release(sbi[2], false);
    bs.release(sbi[3], false);

    // free blocks
    for (size_t i = 0; i < kNumSB; ++i)
        bs.free_swappable_block(sbi[i]);

    delete bs_ptr;
}

void test3()
{
    {
        // ---------- do not free uninitialized block ---------------------
        LOG1 << "next test: do not free uninitialized block";

        block_scheduler_type bs(block_size * sizeof(value_type));
        swappable_block_identifier_type sbi;

        sbi = bs.allocate_swappable_block();
        bs.acquire(sbi);
        bs.release(sbi, false);
        // do not free uninitialized block
    }
    {
        // ---------- do not free initialized block ---------------------
        LOG1 << "next test: do not free initialized block";

        block_scheduler_type bs(block_size * sizeof(value_type));
        swappable_block_identifier_type sbi;

        sbi = bs.allocate_swappable_block();
        bs.acquire(sbi);
        bs.release(sbi, true);
        // do not free initialized block
    }
    if (0) //-tb: causes assertion (which is the expected behaviour)!
    {
        // ---------- do not release but free block ---------------------
        LOG1 << "next test: do not release but free block";

        block_scheduler_type bs(block_size * sizeof(value_type));
        swappable_block_identifier_type sbi;

        sbi = bs.allocate_swappable_block();
        bs.acquire(sbi);
        // do not release block
        bs.free_swappable_block(sbi);
    }
    {
        // ---------- do neither release nor free block ---------------------
        LOG1 << "next test: do neither release nor free block";

        block_scheduler_type bs(block_size * sizeof(value_type));
        swappable_block_identifier_type sbi;

        sbi = bs.allocate_swappable_block();
        bs.acquire(sbi);
        // do not release block
        // do not free block
    }
    if (0) //-tb: causes assertion (which is the expected behaviour)!
    {
        // ---------- release block to often ---------------------
        LOG1 << "next test: release block to often";

        block_scheduler_type bs(block_size * sizeof(value_type));
        swappable_block_identifier_type sbi;

        sbi = bs.allocate_swappable_block();
        bs.acquire(sbi);
        bs.release(sbi, false);
        bs.release(sbi, false); // release once to often
    }
}

int main(int argc, char** argv)
{
    int test_case = -1;
    int internal_memory_megabytes = 256;

    tlx::CmdlineParser cp;

    cp.add_int(
        't', "test-case", "I", test_case,
        "number of the test case to run"
    );
    cp.add_int(
        'm', "memory", "N", internal_memory_megabytes,
        "internal memory to use (in megabytes)"
    );

    cp.set_description("foxxll block_scheduler test");
    cp.set_author("Raoul Steffen, R-Steffen@gmx.de");

    if (!cp.process(argc, argv))
        return -1;

    internal_memory = external_size_type(internal_memory_megabytes) * 1048576;

    // run individual tests

    test1();
    test2();
    test3();

    LOG1 << "end of test";

    return 0;
}

/**************************************************************************/

project(VL_uBMK)

cmake_minimum_required(VERSION 3.1)

if(VL_FOUND)
  include_directories(${VL_INCLUDE_DIR})
endif()

include_directories(./include)

add_microbenchmark(lh_swap_mutex lockhammer.c)
target_compile_definitions(lh_swap_mutex PRIVATE -DATOMIC_TEST="include/swap_mutex.h")
target_link_libraries(lh_swap_mutex -lm)

add_microbenchmark(lh_cas_lockref lockhammer.c)
target_compile_definitions(lh_cas_lockref PRIVATE -DATOMIC_TEST="include/cas_lockref.h")
target_link_libraries(lh_cas_lockref -lm)

add_microbenchmark(lh_cas_rw_lock lockhammer.c)
target_compile_definitions(lh_cas_rw_lock PRIVATE -DATOMIC_TEST="include/cas_rw_lock.h")
target_link_libraries(lh_cas_rw_lock -lm)

add_microbenchmark(lh_event_mutex lockhammer.c)
target_compile_definitions(lh_event_mutex PRIVATE -DATOMIC_TEST="include/mysql/event_mutex.h")
target_link_libraries(lh_event_mutex -lm)

add_microbenchmark(lh_cas_event_mutex lockhammer.c)
target_compile_definitions(lh_cas_event_mutex PRIVATE -DATOMIC_TEST="include/mysql/cas_event_mutex.h")
target_link_libraries(lh_cas_event_mutex -lm)

add_microbenchmark(lh_ticket_spinlock lockhammer.c)
target_compile_definitions(lh_ticket_spinlock PRIVATE -DATOMIC_TEST="include/linux/ticket_spinlock.h")
target_link_libraries(lh_ticket_spinlock -lm)

add_microbenchmark(lh_queued_spinlock lockhammer.c)
target_compile_definitions(lh_queued_spinlock PRIVATE -DATOMIC_TEST="include/linux/queued_spinlock.h")
target_link_libraries(lh_queued_spinlock -lm)

FIND_PACKAGE(PkgConfig)


if(VL_FOUND)
    add_microbenchmark(lh_vlink_lock lockhammer.c)
    target_compile_definitions(lh_vlink_lock PRIVATE -DNTHRDS_READY -DATOMIC_TEST="include/vlink_lock.h")
    target_link_libraries(lh_vlink_lock -lm ${VL_LIBRARY})
else()
    MESSAGE(STATUS "WARNING: No zmq library, skip lh_vlink_lock.")
endif()

if(Boost_LOCKFREE_QUEUE_HPP)
    add_microbenchmark(lh_boost_qlock lockhammer.c)
    target_compile_definitions(lh_boost_qlock PRIVATE -DATOMIC_TEST="include/boost_lock.h")
    target_link_libraries(lh_boost_qlock -lm boost_qlock)
else()
    MESSAGE(STATUS "WARNING: No boost lockfree queue, skip lh_boost_qlock.")
endif()

if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
    ##
    # add general checks so you can use pkg-config for all 
    # the random zmq dependencies that they don't set up in
    # their own pkg-config file 
    ##
    add_microbenchmark(lh_zmq_qlock lockhammer.c)
    if(PkgConfig_FOUND)
        PKG_CHECK_MODULES(SODIUM   IMPORTED_TARGET libsodium)
        PKG_CHECK_MODULES(LIBNORM  IMPORTED_TARGET norm)
        PKG_CHECK_MODULES(LIBPGM   IMPORTED_TARGET openpgm-5.2)
        PKG_CHECK_MODULES(LIBGSS   IMPORTED_TARGET krb5-gssapi)
    endif()
    target_include_directories(lh_zmq_qlock PRIVATE ${ZMQ_INCLUDE_DIR})
    target_link_libraries(lh_zmq_qlock -lm ${ZMQ_LIBRARY})
    if(SODIUM_FOUND)
        target_link_libraries(lh_zmq_qlock PkgConfig::SODIUM)
    endif()
    if(LIBNORM_FOUND)
        target_link_libraries(lh_zmq_qlock PkgConfig::LIBNORM)
    endif()
    if(LIBPGM_FOUND)
        target_link_libraries(lh_zmq_qlock PkgConfig::LIBPGM)
    endif()
    if(LIBGSS_FOUND)
        target_link_libraries(lh_zmq_qlock PkgConfig::LIBGSS)
    endif()
    target_compile_definitions(lh_zmq_qlock PRIVATE -DNTHRDS_READY -DATOMIC_TEST="include/zmq_lock.h")
    set_target_properties(lh_zmq_qlock PROPERTIES LINKER_LANGUAGE CXX)
elseif(NOT (ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND))
  MESSAGE(STATUS "WARNING: No zmq library, skip lh_zmq_qlock.")
endif()
    
if(NOT TARGET lockhammer)
    add_custom_target(lockhammer)
    add_dependencies(lockhammer
        lh_swap_mutex lh_cas_lockref lh_cas_rw_lock lh_event_mutex
        lh_cas_event_mutex lh_ticket_spinlock lh_queued_spinlock)
    if(VL_FOUND)
        add_dependencies(lockhammer lh_vlink_lock)
    endif()
    if(Boost_LOCKFREE_QUEUE_HPP)
        add_dependencies(lockhammer lh_boost_qlock)
    endif()
    if(ZMQ_STATIC_FOUND OR ZMQ_DYNAMIC_FOUND)
        add_dependencies(lockhammer lh_zmq_qlock)
    endif()
endif()

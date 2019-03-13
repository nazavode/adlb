#define OMPI_SKIP_MPICXX

#include <adlb/adlb.h>
#include <mpi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

using payload_size = int;
using handle = int;
using priority = int;
using payload_tag = int;

constexpr payload_tag PAYLOAD_TOKEN = 1;
constexpr payload_tag PAYLOAD_NONE = -11;

using token = std::size_t;
using distance = std::ptrdiff_t;

struct span {
    token start{0};
    distance size{0};
};

std::array<payload_tag, 2> PAYLOAD_TYPES = {PAYLOAD_TOKEN, PAYLOAD_NONE};

constexpr int TARGET_ANY_RANK = -1;

void server() { ::ADLB_Server(5000000, 0); }

void push(const span& domain, int rank) {
    const auto err = ::ADLB_Begin_batch_put(nullptr, 0);
    if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Begin_batch_put failed"};
    struct janitor {
        ~janitor() noexcept { ::ADLB_End_batch_put(); }
    } cleanup;
    for (auto t = domain.start; t < domain.start + domain.size; ++t) {
        const auto err =
            ::ADLB_Put(&t, sizeof(t), TARGET_ANY_RANK, rank, PAYLOAD_TOKEN, 0);
        if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Put failed"};
    }
}

bool pop() {
    int types[2] = {1, -1};
    int recv_handle[ADLB_HANDLE_SIZE];
    payload_tag recv_tag;
    priority recv_prio;
    // handle recv_handle;
    payload_size recv_size;
    int recv_rank;
    // Reserve
    int err =
        ::ADLB_Reserve(types, &recv_tag, &recv_prio, recv_handle, &recv_size, &recv_rank);
    if (err == ADLB_NO_MORE_WORK || err == ADLB_DONE_BY_EXHAUSTION) return false;
    if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Reserve failed"};
    if (recv_tag != PAYLOAD_TYPES[0])
        throw std::runtime_error{"ADLB_Reserve reserved ad unknown type of payload"};
    // Receive
    token recv_token;
    err = ::ADLB_Get_reserved(&recv_token, recv_handle);
    if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Get_reserved failed"};
    return true;
}

template <typename Container>
void stats(const Container& samples, int mpi_root_rank, ::MPI_Comm mpi_comm) {
    using value_type = typename Container::value_type;
    const auto sum =
        std::accumulate(std::begin(samples), std::end(samples), value_type{0});
    const double mean = sum / static_cast<double>(samples.size());
    const double stddev = [&] {
        double accum = 0.0;
        std::for_each(std::begin(samples), std::end(samples),
                      [&](auto d) { accum += (d - mean) * (d - mean); });
        return std::sqrt(accum / (samples.size() - 1));
    }();
    std::array<double, 2> stats_v = {mean, stddev};

    const auto comm_size = [&] {
        int size;
        auto err = ::MPI_Comm_size(mpi_comm, &size);
        if (err != MPI_SUCCESS) throw std::runtime_error{"MPI_Comm_size failed"};
        return size;
    }();

    const auto myrank = [&] {
        int rank;
        auto err = ::MPI_Comm_rank(mpi_comm, &rank);
        if (err != MPI_SUCCESS) throw std::runtime_error{"MPI_Comm_rank failed"};
        return rank;
    }();

    std::vector<double> recv;

    if (myrank == mpi_root_rank) {
        recv.resize(stats_v.size() * comm_size, 0);
    }

    const auto err = ::MPI_Gather(stats_v.data(), stats_v.size(), MPI_DOUBLE, recv.data(),
                                  stats_v.size(), MPI_DOUBLE, mpi_root_rank, mpi_comm);
    if (err != MPI_SUCCESS) throw std::runtime_error{"MPI_Gather failed"};

    if (myrank == mpi_root_rank) {
        std::ostringstream os;
        for (decltype(recv.size()) i = 0; i < recv.size() - 1; i += stats_v.size()) {
            os << std::setfill(' ') << std::setw(4) << std::right << i / stats_v.size()
               << " " << recv[i] << " " << recv[i + 1] << '\n';
        }
        std::cout << os.str() << std::endl;
    }
}

}  // namespace

int main(int argc, char** argv) {
    using clock_t = std::chrono::high_resolution_clock;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ntokens> [nservers] [producer rank]"
                  << std::endl;
        return 1;
    }
    const distance num_tokens = std::stoll(argv[1]);
    const int num_servers = argc >= 3 ? std::stoll(argv[2]) : 1;
    const int producer_rank = argc >= 4 ? std::stoll(argv[3]) : 0;

    if (num_servers <= 0) throw std::runtime_error{"at least one server is mandatory"};

    // MPI
    ::MPI_Init(&argc, &argv);
    struct mpi_janitor {
        ~mpi_janitor() noexcept { ::MPI_Finalize(); }
    } cleanup_mpi;

    const auto world_size = [] {
        int size;
        auto err = ::MPI_Comm_size(MPI_COMM_WORLD, &size);
        if (err != MPI_SUCCESS) throw std::runtime_error{"MPI_Comm_size failed"};
        return size;
    }();
    const int num_workers = world_size - num_servers;
    if (num_workers <= 0)
        throw std::runtime_error{
            "number of servers is too big for MPI_COMM_WORLD, at least one task must be "
            "a worker"};
    if (world_size < 2)
        throw std::runtime_error{
            "at least 2 MPI tasks must be present (1 server, 1 worker)"};
    if (producer_rank >= world_size)
        throw std::runtime_error{
            "specified producer rank is out of MPI_COMM_WORLD's bounds"};

    const auto comm_world_myrank = [] {
        int rank;
        auto err = ::MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if (err != MPI_SUCCESS) throw std::runtime_error{"MPI_Comm_rank failed"};
        return rank;
    }();

    // ADLB
    int am_server;
    int am_debug_server;
    MPI_Comm work_comm;
    const auto err = ::ADLB_Init(num_servers, 0, 0, 1, PAYLOAD_TYPES.data(), &am_server,
                                 &am_debug_server, &work_comm);
    if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Init failed"};

    struct adlb_janitor {
        ~adlb_janitor() noexcept { ::ADLB_Finalize(); }
    } cleanup_adlb;

    if (am_server) {
        server();
    } else {
        if (comm_world_myrank == producer_rank) {
            // Push all tokens at once from a single task,
            // this should force all the servers to balance
            // the load among themselves
            push({0, num_tokens}, comm_world_myrank);
        }
        // Worker
        std::vector<std::uint64_t> samples;
        samples.reserve(num_tokens / num_workers);
        while (true) {
            auto t = clock_t::now();
            bool has_work = pop();
            samples.push_back(
                std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - t)
                    .count());
            if (!has_work) break;
        }

        if (comm_world_myrank == producer_rank) {
            std::cout << "World size : " << world_size << '\n'
                      << "Num servers: " << num_servers << '\n'
                      << "Num tokens : " << num_tokens << std::endl;
        }
        stats(samples, producer_rank, work_comm);
    }
}

#define OMPI_SKIP_MPICXX

#include <adlb/adlb.h>
#include <mpi.h>

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string>

using payload_size = int;
using handle = int;
using priority = int;
using payload_tag = int;

// enum class payload : payload_tag { NONE = -1, TOKEN = 1 };

constexpr payload_tag PAYLOAD_TOKEN = 1;
constexpr payload_tag PAYLOAD_NONE = -11;

using token = std::size_t;
using distance = std::ptrdiff_t;

struct span {
    token start{0};
    distance size{0};
};

namespace {
// constexpr std::array<payload, 2> PAYLOAD_TYPES = {payload::TOKEN, payload::NONE};
std::array<payload_tag, 2> PAYLOAD_TYPES = {PAYLOAD_TOKEN, PAYLOAD_NONE};

constexpr int TARGET_ANY_RANK = -1;
}  // namespace

void server() { ::ADLB_Server(5000000, 0); }

void producer(const span& domain, int rank) {
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

void consumer() {
    payload_tag recv_tag;
    priority recv_prio;
    // handle recv_handle;
    int recv_handle[ADLB_HANDLE_SIZE];
    payload_size recv_size;
    int recv_rank;
    int types[2] = {1, -1};

    int count = 0;
    while (true) {
        // Reserve
        int err = ::ADLB_Reserve(types, &recv_tag, &recv_prio,
                                 recv_handle, &recv_size, &recv_rank);
        if (err == ADLB_NO_MORE_WORK || err == ADLB_DONE_BY_EXHAUSTION) break;
        if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Reserve failed"};
        if (recv_tag != PAYLOAD_TYPES[0])
            throw std::runtime_error{"ADLB_Reserve reserved ad unknown type of payload"};
        // Receive
        token recv_token;
        err = ::ADLB_Get_reserved(&recv_token, recv_handle);
        if (err != ADLB_SUCCESS) throw std::runtime_error{"ADLB_Get_reserved failed"};
        // Work the token (do nothing)
        ++count;
    }
}

int main(int argc, char** argv) {
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
            producer({0, num_tokens}, comm_world_myrank);
        }
        consumer();
    }
}

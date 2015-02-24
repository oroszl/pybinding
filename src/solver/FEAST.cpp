#include "solver/FEAST.hpp"

#ifdef TBM_USE_FEAST
# include "hamiltonian/Hamiltonian.hpp"
# include "support/format.hpp"
using namespace tbm;

template<typename scalar_t>
FEAST<scalar_t>::FEAST(real_t energy_min, real_t energy_max, int subspace_size_guess,
                       bool recycle_subspace, bool is_verbose)
{
    params.energy_min = energy_min;
    params.energy_max = energy_max;
    params.initial_size_guess = subspace_size_guess;
    params.recycled_subspace = recycle_subspace;
    params.is_verbose = is_verbose;
}

template<typename scalar_t>
void FEAST<scalar_t>::v_solve()
{
    // size of the matrix
    params.system_size = hamiltonian->get_matrix().rows();
    // reset info flags
    info.recycle_warning = false;
    info.recycle_warning_loops = 0;
    info.size_warning = false;
    
    // call the solver
    call_feast();
    
    // check for errors in case of recycled subspace
    if (params.recycled_subspace)
    {
        while (info.refinement_loops >= params.max_refinement_loops || info.return_code == 3)
        { // refinement loop count is greater than allowed or subspace is too small
            info.recycle_warning = true;
            
            // make sure we don't do this forever
            info.recycle_warning_loops += info.refinement_loops;
            if (info.recycle_warning_loops > 2*params.max_refinement_loops)
                throw std::runtime_error{"FEAST: failed to converge within desired loop count."};
            
            // clearData() will increase suggested_size back to initial_size_guess
            // but if that was already the case, try to increase the subspace size
            if (info.suggested_size == params.initial_size_guess)
                params.initial_size_guess *= 1.7;
            force_clear();
            // rerun calculation with cleared data
            call_feast();
        }
    }

    // check for any return code errors
    if (info.return_code != 0)
    {
        if (info.return_code == 3)
        { // FEAST error: Subspace guess M0 is too small
            info.size_warning = true;
            while (info.return_code == 3)
            { // try to increase the subspace size and rerun calculation
                params.initial_size_guess *= 1.7;
                force_clear();
                call_feast();
                
                // ran into a different error while trying to recover
                if (info.return_code != 3 && info.return_code != 0)
                    throw std::runtime_error{"FEAST: Subspace guess is too small. Failed to recover."};
            }
        }
        else if (info.return_code == 1)
        { /*pass*/ } // not really an error: "No eigenvalues found in the given energy range."
        else
            throw std::runtime_error{"FEAST error code: " + std::to_string(info.return_code)};
    }
    
    info.max_residual = residual.head(info.final_size).maxCoeff();
    if (info.recycle_warning)
        info.refinement_loops += info.recycle_warning_loops;
}

template<typename scalar_t>
std::string FEAST<scalar_t>::v_report(bool is_shortform) const
{
    using fmt::format;
    std::string report;
    
    if (info.size_warning)
        report += format("Resized initial guess: {}\n", params.initial_size_guess);

    std::string fmt;
    if (is_shortform)
    {
        fmt = "Subspace({final_size}|{suggested_size}|{ratio:.2f}), "
            "Refinement({loops}|{err:.2e}|{residual:.2e})";
    }
    else
    {
        fmt = "Final subspace size is {final_size} | "
            "Suggested size is {suggested_size} ({ratio:.2f} ratio)\n"
            "Converged after {loops} refinement loop(s)\n"
            "Error trace: {error_trace:.2e} | Max. residual: {residual:.2e}\n"
            "\nCompleted in";
    }

    report += format(
        fmt, info.final_size, info.suggested_size, (float)info.suggested_size / info.final_size,
        info.refinement_loops, info.error_trace, info.max_residual
    );

    return report;
}

template<typename scalar_t>
void FEAST<scalar_t>::clear()
{
    is_solved = false;
    if (params.recycled_subspace == false)
        force_clear();
}

template<typename scalar_t>
void FEAST<scalar_t>::force_clear()
{
    _eigenvalues.resize(0);
    _eigenvectors.resize(0, 0);
    residual.resize(0);
}

template<typename scalar_t>
void FEAST<scalar_t>::init_feast()
{
    feastinit(fpm);
    fpm[0] = params.is_verbose ? 1 : 0;
    
    // the subspace can only be recycled if we actually have data to recycle
    int can_recycle = (_eigenvalues.size() != 0) ? 1 : 0;
    fpm[4] = params.recycled_subspace ? can_recycle : 0;
    
    fpm[1] = params.contour_points;
    fpm[2] = params.dp_stop_criteria;
    fpm[3] = params.max_refinement_loops;
    fpm[5] = params.residual_convergence ? 1 : 0;
    fpm[6] = params.sp_stop_criteria;
}

template<typename scalar_t>
void FEAST<scalar_t>::init_pardiso()
{
    fpm[63] = 0; // disabled
    int* iparm = &fpm[64];

    iparm[0] = 1; // use non-defaults
    iparm[1] = 2; // *** try 3
    // 2 // _reserved
    iparm[3] = 0; // preconditioned CGS/CG
    iparm[4] = 0; // user permutation // must be 0
    iparm[5] = 0; // write solution on x
    // 6 // _output
    iparm[7] = 0; // iterative refinement steps // best perf. 0
    // 8 // _reserved
    iparm[9] = 8;
    iparm[10] = 0; // scaling vectors
    iparm[11] = 0; // solve with transposed or conjugate transposed
    iparm[12] = 1; // matching
    // 13-16 // _output
    iparm[17] = 0; // report the number of non-zeros in the factors (-1)
    iparm[18] = 0; // more reporting
    // 19 // _output
    iparm[20] = 1; // pivoting // best perf. 1 for real
    // 21-22 // _output
    iparm[23] = 1; // *** parallel 
    iparm[24] = 0; // *** parallel
    // 25 // _reserved
    iparm[26] = 0; // check for index errors
    iparm[27] = 0; // 1 for single precision
    // 28 // _reserved
    // 29 // _output
    iparm[30] = 0; // partial solve and computing selected components of the solution vectors
    // 31-32 // _reserved
    iparm[33] = 0; // CNR // 2 is good for some reason
    iparm[34] = 0; // zero-base indexing
    // 35-58 // _reserved
    iparm[59] = 0; // out-of-core mode
    // 60-61 // _reserved
    // 62 // _output
    // 63 // _reserved
}

template<typename scalar_t>
void FEAST<scalar_t>::call_feast()
{
    init_feast();
//    init_pardiso();
    
    // prepare resources for the results
    if (_eigenvalues.size() == 0)
    {
        // make sure the subspace isn't bigger than the system (or negative)
        if (params.initial_size_guess > params.system_size || params.initial_size_guess < 0)
            params.initial_size_guess = params.system_size;
        
        _eigenvalues.resize(params.initial_size_guess);
        info.suggested_size = params.initial_size_guess;
    }
    if (residual.size() == 0)
        residual.resize(params.initial_size_guess);

    if (_eigenvectors.size() == 0)
        _eigenvectors.resize(params.system_size, params.initial_size_guess);

    // solve real or complex Hamiltonian
    call_feast_impl();
}

template<typename scalar_t>
void FEAST<scalar_t>::call_feast_impl()
{
    
}

template<>
void FEAST<float>::call_feast_impl()
{
    const auto& h_matrix = hamiltonian->get_matrix();
    
    // convert to one-based index
    ArrayX<int> cols(h_matrix.nonZeros());
    ArrayX<int> rows(h_matrix.rows() + 1);

    for (int i = 0; i < h_matrix.nonZeros(); i++)
        cols[i] = h_matrix.innerIndexPtr()[i] + 1;
    for (int i = 0; i < h_matrix.rows() + 1; i++)
        rows[i] = h_matrix.outerIndexPtr()[i] + 1;

    sfeast_scsrev(
        &params.matrix_format,      // (in) full matrix
        &params.system_size,		// (in) size of the matrix
        h_matrix.valuePtr(),        // (in) sparse matrix values
        rows.data(),    		    // (in) rows (outer starts)
        cols.data(),    		    // (in) cols (inner index)
        fpm,		    		    // (in) FEAST parameters
        &info.error_trace, 		    // (out) relative error on trace
        &info.refinement_loops,	    // (out) the number of refinement loops executed
        &params.energy_min,    	    // (in) lower bound
        &params.energy_max,    	    // (in) upper bound
        &info.suggested_size,       // (in/out) subspace size guess
        _eigenvalues.data(),        // (out) eigenvalues
        _eigenvectors.data(),       // (in/out) eigenvectors
        &info.final_size,           // (out) total number of eigenvalues found
        residual.data(),        	// (out) relative residual vector (must be length of M0)
        &info.return_code           // (out) info or error code
    );
}

template<>
void FEAST<std::complex<float>>::call_feast_impl()
{
    const auto& h_matrix = hamiltonian->get_matrix();
    
    // convert to one-based index
    ArrayX<int> cols(h_matrix.nonZeros());
    ArrayX<int> rows(h_matrix.rows() + 1);

    for (int i = 0; i < h_matrix.nonZeros(); i++)
        cols[i] = h_matrix.innerIndexPtr()[i] + 1;
    for (int i = 0; i < h_matrix.rows() + 1; i++)
        rows[i] = h_matrix.outerIndexPtr()[i] + 1;

    cfeast_hcsrev(
        &params.matrix_format,      // (in) full matrix
        &params.system_size,		// (in) size of the matrix
        (MKL_Complex8*)h_matrix.valuePtr(), // (in) sparse matrix values
        rows.data(),		        // (in) rows (outer starts)
        cols.data(),		        // (in) cols (inner index)
        fpm,				        // (in) FEAST parameters
        &info.error_trace, 		    // (out) relative error on trace
        &info.refinement_loops,      // (out) the number of refinement loops executed
        &params.energy_min,	        // (in) lower bound
        &params.energy_max,	        // (in) upper bound
        &info.suggested_size,        // (in/out) subspace size guess
        _eigenvalues.data(),         // (out) eigenvalues
        (MKL_Complex8*)_eigenvectors.data(), // (in/out) eigenvectors
        &info.final_size,            // (out) total number of eigenvalues found
        residual.data(),	        // (out) relative residual vector (must be length of M0)
        &info.return_code            // (out) info or error code
    );
}



template<typename scalar_t>
std::unique_ptr<Solver> FEASTFactory::try_create_for(
    const std::shared_ptr<const Hamiltonian>& hamiltonian) const
{
    auto cast_ham = std::dynamic_pointer_cast<const HamiltonianT<scalar_t>>(hamiltonian);
    if (!cast_ham)
        return nullptr;

    std::unique_ptr<FEAST<scalar_t>> feast{
        new FEAST<scalar_t>(energy_min, energy_max, subspace_size, recycle_subspace, is_verbose)
    };
    feast->set_hamiltonian(cast_ham);
    
    feast->params.contour_points = params.contour_points;
    feast->params.max_refinement_loops = params.max_refinement_loops;
    feast->params.sp_stop_criteria = params.sp_stop_criteria;
    feast->params.dp_stop_criteria = params.dp_stop_criteria;
    feast->params.residual_convergence = params.residual_convergence;
    
    return std::move(feast);
}

std::unique_ptr<Solver>FEASTFactory::create_for(
    const std::shared_ptr<const Hamiltonian>& hamiltonian) const
{
    std::unique_ptr<Solver> new_solver;
    
    if (!new_solver) new_solver = try_create_for<float>(hamiltonian);
    if (!new_solver) new_solver = try_create_for<std::complex<float>>(hamiltonian);
    if (!new_solver) new_solver = try_create_for<double>(hamiltonian);
//    if (!new_solver) new_solver = try_create_for<std::complex<double>>(hamiltonian);
    if (!new_solver)
        throw std::runtime_error{"KPMFactory: unknown Hamiltonian type."};
    
    return new_solver;
}

FEASTFactory& FEASTFactory::advanced(int points, int loops, int sp, int dp, bool stop_residual)
{
    params.contour_points = points;
    params.max_refinement_loops = loops;
    params.sp_stop_criteria = sp;
    params.dp_stop_criteria = dp;
    params.residual_convergence = stop_residual;
    
    return *this;
}

#endif // TBM_USE_FEAST
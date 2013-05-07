#ifndef _PLANE_PARAMETERS_ESTIMATOR_TXX_
#define _PLANE_PARAMETERS_ESTIMATOR_TXX_

#include <math.h>
#include <vnl/algo/vnl_svd.h>
#include <vnl/algo/vnl_symmetric_eigensystem.h>
#include "Epsilon.h"
#include "PlaneParametersEstimator.h"

namespace lsqrRecipes {

template< unsigned int dimension >
PlaneParametersEstimator<dimension>::PlaneParametersEstimator(double delta) : 
  ParametersEstimator< Point<double, dimension>, double>(dimension) 
{
  this->deltaSquared = delta*delta;
}
/*****************************************************************************/
/*
 * Estimate the plane parameters  [n_0,...,n_k,a_0,...,a_k].
 * The plane is given as dot(n,p-a) = dot(n,p) - dot(n,a) = 0
 * The second dot product is a constant, d, so each point gives us one
 * equation in the equation system (Ax=0):
 *                        [n_0] 
 *                          .
 *       [p_0,...,p_k,-1]   .   =  0
 *                          .
 *                        [n_k]
 *                        [ d ] 
 *
 * If all k+1 points are linearly independent then the matrix A has a one
 * dimensional null space [A is a (k+1)X(k+2) matrix] which is the answer we seek.
 * 
 */
template< unsigned int dimension  >
void PlaneParametersEstimator<dimension>::estimate(std::vector< Point<double, dimension> *> &data, 
										           std::vector<double> &parameters)
{
	unsigned int i, j;
	double norm;
	
	parameters.clear();
	              //user forgot to initialize the minimal number of required 
                //elements or there are not enough data elements for computation
	if( this->minForEstimate==0 || data.size() < this->minForEstimate )
		return;
	
	if( dimension == 3 ) { //compute plane normal directly
    double nx,ny,nz;
    vnl_vector<double> v1(3), v2(3);
  
    v1[0] = (*data[1])[0] - (*data[0])[0];
    v1[1] = (*data[1])[1] - (*data[0])[1];
    v1[2] = (*data[1])[2] - (*data[0])[2];
    v2[0] = (*data[2])[0] - (*data[0])[0];
    v2[1] = (*data[2])[1] - (*data[0])[1];
    v2[2] = (*data[2])[2] - (*data[0])[2];
  
    nx = v1[1]*v2[2] - v1[2]*v2[1];
    ny = v1[2]*v2[0] - v1[0]*v2[2];
    nz = v1[0]*v2[1] - v1[1]*v2[0];
	  norm = sqrt(nx*nx+ny*ny+nz*nz);
    
    if(norm< EPS) //points are collinear
      return;
	  parameters.push_back(nx/norm);
	  parameters.push_back(ny/norm);
	  parameters.push_back(nz/norm);
	}
	else { //get the plane normal as the null space of the matrix described above	
	  vnl_matrix<double> A( this->minForEstimate,
                          this->minForEstimate+1 );

	  for( i=0; i<this->minForEstimate; i++ ) {
		  Point<double, dimension> &pnt = *(data[i]);
		  for( j=0; j<this->minForEstimate; j++ )
			  A(i,j) = pnt[j];
		  A(i,j) = -1;
	  }

	  vnl_svd<double> svdA( A );
                 //explicitly zero out small singular values 
    svdA.zero_out_absolute( EPS );
	        //the points are linearly dependent, need at least k linearly 
          //independent points (gives us rank(A)=k)
	  if( svdA.rank()<this->minForEstimate )
		  return;

	               //the one dimensional null space of A is the solution we seek
	  vnl_vector<double> x(this->minForEstimate+1);
	  x = svdA.nullvector();
            //get the hyperplane normal, we need to set it so ||n||=1, this 
            //means we need to scale our solution to be 
            // 1/||n_computed||*[n_computed,d] which is also a solution to the 
            //equation system.
    norm = 0;
    for( i=0; i<this->minForEstimate; i++ ) {
      norm+=x[i]*x[i];
		  parameters.push_back( x[i] );
    }
    norm = 1.0/sqrt(norm);
    for( i=0; i<this->minForEstimate; i++ )
      parameters[i]*=norm;
  }
                        //first point is arbitrarily chosen to be the 
                        //"point on plane"
	for( i=0; i<dimension; i++ )
		parameters.push_back( (*data[0])[i] );
}
/*****************************************************************************/
/*
 * Estimate the plane parameters  [n_0,...,n_k,a_0,...,a_k].
 */
template< unsigned int dimension  >
void PlaneParametersEstimator<dimension>::estimate(std::vector< Point<double, dimension> > &data, 
                                                   std::vector<double> &parameters)
{
	std::vector< Point<double, dimension> *> usedData;
	unsigned int dataSize = static_cast<unsigned int>(data.size());
	for(unsigned int i=0; i<dataSize; i++)
		usedData.push_back(&(data[i]));
	estimate(usedData,parameters);
}
/*****************************************************************************/
/*
 * Estimate the plane parameters  [n_0,...,n_k,a_0,...,a_k].
 */
template< unsigned int dimension  >
void PlaneParametersEstimator<dimension>::leastSquaresEstimate(std::vector< Point<double, dimension> *> &data, 
                                                               std::vector<double> &parameters)
{
	parameters.clear();	   //not enough data elements for computation
	if(data.size()<this->minForEstimate)
		return;

  unsigned int i, j, k, pointNum = static_cast<unsigned int>(data.size());
  vnl_matrix<double> meanMat(dimension, dimension), covariance(dimension,dimension,0);
  vnl_vector<double> mean(dimension,0);

               //create covariance matrix
  double sqrtN = sqrt((double)pointNum);
  for(i=0; i<pointNum; i++) 
    for(j=0; j<dimension; j++)
      mean[j] += (*data[i])[j];
  mean/= sqrtN;
  for(i=0; i<dimension; i++)
    for(j=i; j<dimension; j++)
       meanMat(i,j) = meanMat(j,i) = mean[i]*mean[j];

                  //upper half
  for(i=0; i<pointNum; i++) 
    for(j=0; j<dimension; j++)
      for(k=j; k<dimension; k++)
        covariance(j,k) += (*data[i])[j] * (*data[i])[k];
                 //copy to lower half
  for(j=0; j<dimension; j++)
    for(k=j+1; k<dimension; k++)
      covariance(k,j) = covariance(j,k);
               //subtract mean matrix
  covariance-=meanMat;

                           //compute eigenvectors/values of covariance 
  vnl_symmetric_eigensystem<double> eigenSystem(covariance);
  
                //the hyperplane normal is the eigenvector corresponding to 
                //the smallest eigenvalue
                //I assume ||eigenSystem.V(i,0)|| = 1
  for(i=0; i<dimension; i++)
    parameters.push_back(eigenSystem.V(i,0));
  for(i=0; i<dimension; i++)
    parameters.push_back(mean[i]/sqrtN);   
}
/*****************************************************************************/
/*
 * Estimate the plane parameters  [n_0,...,n_k,a_0,...,a_k].
 */
template< unsigned int dimension  >
void PlaneParametersEstimator<dimension>::leastSquaresEstimate(std::vector< Point<double, dimension> > &data, 
														       std::vector<double> &parameters)
{
	std::vector< Point<double, dimension> *> usedData;
	unsigned int dataSize = static_cast<unsigned int>(data.size());
	for(unsigned int i=0; i<dataSize; i++)
		usedData.push_back(&(data[i]));
	leastSquaresEstimate(usedData,parameters);
}
/*****************************************************************************/
/*
 * Given the the plane parameters  [n_0,...,n_k,a_0,...,a_k] check if
 * dot([n_0,...,n_k], [data[0]-a_0,...,data[k]-a_k]) < delta
 *
 * If the parameters vector is too short an exception will
 * be thrown by the vector's [] operator. We don't check vector length explicitly.
 */
template< unsigned int dimension  >
bool PlaneParametersEstimator<dimension>::agree(std::vector<double> &parameters, 
                                                Point<double, dimension> &data)
{
  double signedDistance = 0;
  for(unsigned int i=0; i<dimension; i++)
	  signedDistance += parameters[i]*(data[i]-parameters[dimension+i]);
	return ((signedDistance*signedDistance) < this->deltaSquared);
}

} //namespace lsqrRecipes

#endif //_PLANE_PARAMETERS_ESTIMATOR_TXX_

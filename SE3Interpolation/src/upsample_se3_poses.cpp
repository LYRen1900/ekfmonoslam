#include "se3_fitting.h"
#include <algorithm> // count
#include <fstream>
#include <iomanip>

int main(int argc, const char* argv[]) {
    typedef double Scalar;

    typedef Sophus::SE3Group<Scalar> SE3Type;
    typedef Eigen::Matrix<Scalar, 10, 1> TAO; // time, acceleration(\ddot{t}_s^w), angular rate(\omega_{is}^s), and velocity((\dot{t}_s^w))

    std::string datasetPath;
    std::string poseFile; // each row: time[sec] tx[m] ty[m] tz[m] qx qy qz qw of sensor to world transform
    std::string samplePoseFile; // each row as above
    std::string inertialSampleFile; // each row: time[sec], gx[rad/s], gy, gz, ax, ay, az[m/s^2],
    // angular rate is \omega_{is}^s, acceleration is \ddot{t}_s^w

    Scalar outputFreq = 400;
    if (argc < 2) {
      std::cerr << "Example Usage:" << argv[0] << " poseTxtFile inertialSampleOutputCsvFile outputFreq" << std::endl;
      std::cerr << "For better accuracy in later-on correlation, outputFreq "
                   "is at least twice of the fastest measurement frequency" << std::endl;
      return 1;
    }
    
    if (argc > 1) {
      poseFile = argv[1];
      const size_t last_slash_idx = poseFile.find_last_of("/\\");
      if (std::string::npos != last_slash_idx)
      {
          datasetPath = poseFile.substr(0, last_slash_idx);
      }
    }

    if (argc > 2)
      inertialSampleFile = argv[2];
    else
      inertialSampleFile = datasetPath + "/UpsampledPseudoImu.csv";

    if (argc > 3)
      outputFreq = std::atof(argv[3]);
    

    std::vector<SE3Type, Eigen::aligned_allocator<SE3Type> > q02n; // q_0^w, q_1^w, ..., q_n^w; N=n+1 poses, sensor frame to world frame transformations
    std::vector<Scalar> times; // timestamps for N poses

    samplePoseFile = datasetPath + "/UpsampledFrameTrajectory.txt";
    
    q02n.reserve(30*100);
    times.reserve(30*100);
    // read poses
    std::ifstream dataptr(poseFile);
    assert(!dataptr.fail());
    Eigen::Matrix<Scalar,8,1> transMat;
    Scalar precursor=0;
    int lineNum=0;
    std::string line;
    char input_delimiter = '\0';
    while(std::getline(dataptr, line)) {
        if (line.find('%') != std::string::npos)
            continue;
        if (line.length() < 8)
            break;
        if (input_delimiter == '\0') {
            size_t num_comma = std::count(line.begin(), line.end(), ',');
            if (num_comma > 6)
                input_delimiter = ',';
            else
                input_delimiter = ' ';
        }
        std::stringstream stream(line);
        stream>>precursor;
        
        transMat[0]=precursor;
        if (input_delimiter == ' ') {
            for (int j=1; j<8; ++j)
                stream >> transMat[j];
        } else {
            char trash_delimiter = ',';
            for (int j=1; j<8; ++j)
                stream >> trash_delimiter >> transMat[j];
        }
        ++lineNum;
        times.push_back(transMat[0]);
        Eigen::Quaternion<Scalar> q_ws(transMat[7], transMat[4], transMat[5], transMat[6]);
        Eigen::Matrix<Scalar, 3, 1> t_ws(transMat[1], transMat[2], transMat[3]);
        q02n.push_back(SE3Type(q_ws, t_ws));
    }
    dataptr.close();
    std::cout << "Load #poses:" << lineNum << " from " << poseFile << std::endl;

    std::vector<Eigen::Matrix<Scalar, 4,4 >, Eigen::aligned_allocator<Eigen::Matrix<Scalar, 4, 4> > > samplePoses;
    std::vector<TAO, Eigen::aligned_allocator<TAO>> samples;
    InterpolateIMUData(q02n, times, outputFreq, samplePoses,  samples);

    // save inertial data
    std::ofstream sampleptr(inertialSampleFile);
    sampleptr << "%%timestamp(sec), $\\Omega_{is}^s$(rad/sec), $a_{is}^w$(m/s^2)" << std::endl;

    int dataCount = samples.size();
    std::cout << "Interpolated " << dataCount << " pseudo inertial data at " << outputFreq << " Hz" << std::endl;

    char delimiter = ',';

    for (int i=0; i<dataCount;++i) {
        sampleptr << std::setprecision(20) << std::defaultfloat << samples[i][0] << delimiter;
        sampleptr << std::setprecision(6);
        for (int j=0; j<3; ++j)
            sampleptr << samples[i][j+4] << delimiter;
        for (int j=0; j<2; ++j)
            sampleptr << samples[i][j+1] << delimiter;
        sampleptr << samples[i][3] << std::endl;
    }
    sampleptr.close();
    std::cout << "Interpolated pseudo inertial data saved to " << inertialSampleFile << std::endl;
    return 0;
}

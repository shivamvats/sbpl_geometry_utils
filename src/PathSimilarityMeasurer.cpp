#include <sbpl_geometry_utils/PathSimilarityMeasurer.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace sbpl
{

PathSimilarityMeasurer::~PathSimilarityMeasurer()
{
}

double PathSimilarityMeasurer::measure(const std::vector<const Trajectory*>& trajectories,
                                       int numWaypoints)
{
    // check for invalid number of waypoints or empty list of trajectories
    if (numWaypoints < 2 || trajectories.size() == 0) {
        return -1.0;
    }

    // check to make sure all trajectories are non-NULL
    for (int i = 0; i < (int)trajectories.size(); i++) {
        if (trajectories[i] == NULL) {
            return -1.0;
        }
    }

    std::vector<double> pathLengths(trajectories.size(), 0);

    // calculate pathLengths
    for (unsigned i = 0; i < trajectories.size(); i++) {
        pathLengths[i] = calcPathLength(*(trajectories[i]));
    }

    // calculate a new trajectories representing the old trajectories but having the same amount of
    // waypoints each
    std::vector<Trajectory> newTrajs;
    newTrajs.reserve(trajectories.size());
    for (unsigned i = 0; i < trajectories.size(); i++) {
        newTrajs.push_back(Trajectory());
        generateNewWaypoints(*trajectories[i], pathLengths[i], numWaypoints, newTrajs[i]);
    }

    // print out the new generated paths to make sure it makes sense
    std::cout << "\t{ numwaypoints = " << numWaypoints << std::endl;;
    for (int i = 0; i < (int)newTrajs.size(); i++) {

        // print out the trajectory
        std::cout << "\t\t{";
        for (int j = 0; j < (int)newTrajs[i].size(); j++) {
            std::cout << newTrajs[i][j];
            if (j != (int)newTrajs[i].size() - 1) std::cout << ", ";
        }
        std::cout << "}" << std::endl;
    }
    std::cout << "\t}" << std::endl;

    // make sure we actually have numWaypoints for each path
    for (int i = 0; i < (int)newTrajs.size(); i++) {
        assert((int)newTrajs[i].size() == numWaypoints);
    }

    Trajectory avgTrajectory;
    avgTrajectory.reserve(numWaypoints);

    // calculate the average trajectory for this set of trajectories
    for (int i = 0; i < numWaypoints; i++) {
        geometry_msgs::Point avg;
        avg.x = 0.0; avg.y = 0.0; avg.z = 0.0;

        for (int j = 0; j < (int)newTrajs.size(); j++) {
            avg.x += newTrajs[j][i].x;
            avg.y += newTrajs[j][i].y;
            avg.z += newTrajs[j][i].z;
        }

        avg.x /= newTrajs.size();
        avg.y /= newTrajs.size();
        avg.z /= newTrajs.size();
        avgTrajectory.push_back(avg);
    }

    std::vector<double> variances;
    variances.reserve(numWaypoints);
    for (int i = 0; i < numWaypoints; i++) {
        double variance = 0.0;
        for (int j = 0; j < (int)newTrajs.size(); j++) {
            variance += distSqrd(newTrajs[j][i], avgTrajectory[i]);
        }
        variances.push_back(variance);
    }

    double totalVariance = 0.0;
    for (int i = 0; i < numWaypoints; i++) {
        totalVariance += variances[i];
    }

    return totalVariance;
}

double PathSimilarityMeasurer::measureDTW(const std::vector<const Trajectory*>& trajectories, int numWaypoints)
{
    // check for invalid number of waypoints or empty list of trajectories
    if (numWaypoints < 2 || trajectories.size() == 0) {
        return -1.0;
    }

    // check to make sure all trajectories are non-NULL
    for (int i = 0; i < (int)trajectories.size(); i++) {
        if (trajectories[i] == NULL) {
            return -1.0;
        }
    }

    std::vector<double> pathLengths(trajectories.size(), 0);

    // calculate pathLengths
    for (unsigned i = 0; i < trajectories.size(); i++) {
        pathLengths[i] = calcPathLength(*(trajectories[i]));
    }

    // calculate a new trajectories representing the old trajectories but having the same amount of
    // waypoints each
    std::vector<Trajectory> newTrajs;
    newTrajs.reserve(trajectories.size());
    for (unsigned i = 0; i < trajectories.size(); i++) {
        newTrajs.push_back(Trajectory());
        generateNewWaypoints(*trajectories[i], pathLengths[i], numWaypoints, newTrajs[i]);
    }

    // make sure we actually have numWaypoints for each path
    for (int i = 0; i < (int)newTrajs.size(); i++) {
        assert((int)newTrajs[i].size() == numWaypoints);
    }

    // allocate memory for dynamic programming
    const double DTW_INFINITE = std::numeric_limits<double>::max();
    double** dtw = new double*[numWaypoints];
    for (int i = 0; i < numWaypoints; i++) {
        dtw[i] = new double[numWaypoints];
    }

    // initialize dtw
    for (int i = 1; i < numWaypoints; i++) {
        dtw[0][i] = DTW_INFINITE;
        dtw[i][0] = DTW_INFINITE;
    }
    dtw[0][0] = 0.0;

    for (int i = 1; i < numWaypoints; i++) {
        for (int j = 1; j < numWaypoints; j++) {
            const geometry_msgs::Point& p1 = newTrajs[0][i];
            const geometry_msgs::Point& p2 = newTrajs[1][j];
            // calculate the average point between point i on path one and point j on path two
            // TODO: calculate the average point for all paths instead of just the first two
            geometry_msgs::Point averagePt;
            averagePt.x = averagePt.y = averagePt.z = 0.0;
            averagePt.x = (p1.x + p2.x) / 2.0;
            averagePt.y = (p1.y + p2.y) / 2.0;
            averagePt.z = (p1.z + p2.z) / 2.0;

            // cost between points is the computed as their variance from the midpoint
            double cost = distSqrd(p1, averagePt) + distSqrd(p2, averagePt);

            // dynamic programming funkiness
            dtw[i][j] = cost + std::min(dtw[i - 1][j - 1], std::min(dtw[i - 1][j], dtw[i][j - 1]));
        }
    }

    double similarity = dtw[numWaypoints - 1][numWaypoints - 1];

    for (int i = 0; i < numWaypoints; i++) {
        delete dtw[i];
    }
    delete dtw;

    return similarity;
}

PathSimilarityMeasurer::PathSimilarityMeasurer()
{
}

double PathSimilarityMeasurer::euclidDist(const geometry_msgs::Point& p1, const geometry_msgs::Point& p2)
{
    return sqrt(distSqrd(p1, p2));
}

double PathSimilarityMeasurer::distSqrd(const geometry_msgs::Point& p1, const geometry_msgs::Point& p2)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;
    return dx * dx + dy * dy + dz * dz;
}

double PathSimilarityMeasurer::calcPathLength(const Trajectory& traj)
{
    if (traj.size() < 2) {
        return 0.0;
    }

    double pathLength = 0.0;
    for (unsigned i = 1; i < traj.size(); i++) {
        const geometry_msgs::Point& p1 = traj[i - 1];
        const geometry_msgs::Point& p2 = traj[i];
        pathLength += euclidDist(p1, p2);
    }
    return pathLength;
}

bool PathSimilarityMeasurer::generateNewWaypoints(const Trajectory& traj, double pathLength,
                                                  int numWaypoints, Trajectory& newTraj)
{
    if (numWaypoints < 2) {
        newTraj.clear();
        return false;
    }

    newTraj.reserve(numWaypoints);

    double pathInc = pathLength / (numWaypoints - 1);

    // keep the same start point
    newTraj.push_back(traj.front());
    int numPlacedWaypoints = 1;

    int passedWaypoint = 0;

    // generate the new waypoints
    while (numPlacedWaypoints != numWaypoints) {
        // push back the last waypoint and return
        if (numPlacedWaypoints == numWaypoints - 1) {
            newTraj.push_back(traj.back());
            break;
        }

        double distBetweenActualWaypoints = euclidDist(traj[passedWaypoint], traj[passedWaypoint + 1]);
        double distUntilNextWaypoint = euclidDist(newTraj.back(), traj[passedWaypoint + 1]);

        if (pathInc < distUntilNextWaypoint) {
            double alpha = pathInc / distUntilNextWaypoint;

            // calculate the point between our most latest waypoint and the next one we want
            // to generate
            double x = (1.0 - alpha) * newTraj.back().x + alpha * traj[passedWaypoint + 1].x;
            double y = (1.0 - alpha) * newTraj.back().y + alpha * traj[passedWaypoint + 1].y;
            double z = (1.0 - alpha) * newTraj.back().z + alpha * traj[passedWaypoint + 1].z;
            geometry_msgs::Point p;
            p.x = x; p.y = y; p.z = z;

            newTraj.push_back(p);
            numPlacedWaypoints++;
        }
        else {
            passedWaypoint++; // we passed another of the actual waypoints
            distBetweenActualWaypoints = euclidDist(traj[passedWaypoint], traj[passedWaypoint + 1]);

            double newDist = pathInc - distUntilNextWaypoint;
            double alpha = newDist / distBetweenActualWaypoints;

            double x = (1.0 - alpha) * traj[passedWaypoint].x + alpha * traj[passedWaypoint + 1].x;
            double y = (1.0 - alpha) * traj[passedWaypoint].y + alpha * traj[passedWaypoint + 1].y;
            double z = (1.0 - alpha) * traj[passedWaypoint].z + alpha * traj[passedWaypoint + 1].z;
            geometry_msgs::Point p;
            p.x = x; p.y = y; p.z = z;

            newTraj.push_back(p);
            numPlacedWaypoints++;
        }
    }

    return true;
}

std::ostream& operator<<(std::ostream& o, const geometry_msgs::Point& p)
{
    o << "{x: " << p.x << ",  y: " << p.y << ", z: " << p.z << "}";
    return o;
}

}

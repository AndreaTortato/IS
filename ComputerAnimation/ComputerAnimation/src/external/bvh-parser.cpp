#include "bvh-parser.h"
#include "../math/quat.h"
#define MULTI_HIERARCHY 0

namespace bvh {

    void Bvh::recalculateJoints(std::shared_ptr<Joint> start_joint) {

        if (start_joint == NULL)
        {
            if (root_joint_ == NULL)
                return;
            else
                start_joint = root_joint_;
        }

        //LOG(DEBUG) << "recalculate_joints_ltm: " << start_joint->name();

        mat4 offmat_backup;
        offmat_backup.position = vec4(start_joint->offset().x, start_joint->offset().y, start_joint->offset().z, 1);

        std::vector<std::vector<float>> data = start_joint->channel_data();

        for (int i = 0; i < num_frames_; i++) {
            mat4 offmat = offmat_backup; // offset matrix
            mat4 rmat;  // identity matrix set on rotation matrix
            mat4 tmat;  // identity matrix set on translation matrix

            for (int j = 0; j < start_joint->channels_order().size(); j++) {
                if (start_joint->channels_order()[j] == Joint::Channel::XPOSITION)
                    tmat.position.x = tmat.position.x + data[i][j];
                else if (start_joint->channels_order()[j] == Joint::Channel::YPOSITION)
                    tmat.position.y = tmat.position.y + data[i][j];
                else if (start_joint->channels_order()[j] == Joint::Channel::ZPOSITION)
                    tmat.position.z = tmat.position.z + data[i][j];
                else if (start_joint->channels_order()[j] == Joint::Channel::XROTATION)
                    rmat = rmat * quatToMat4(angleAxis(data[i][j], vec3(1, 0, 0)));

                else if (start_joint->channels_order()[j] == Joint::Channel::YROTATION)
                    rmat = rmat * quatToMat4(angleAxis(data[i][j], vec3(0, 1, 0)));
                else if (start_joint->channels_order()[j] == Joint::Channel::ZROTATION)
                    rmat = rmat * quatToMat4(angleAxis(data[i][j], vec3(0, 0, 1)));
            }

            mat4 ltm; // local transformation matrix

            if (start_joint->parent() != NULL)
                ltm = start_joint->parent()->ltm(i) * offmat;
            else
                ltm = tmat * offmat;

            start_joint->set_pos(vec3(ltm.position.x, ltm.position.y, ltm.position.z));
            //LOG(TRACE) << "Joint world position: " << ltm.position.x << ", " << ltm.position.y << ", " << ltm.position.z;

            ltm = ltm * rmat;

            //LOG(TRACE) << "Local transformation matrix: \n" << utils::mat4tos(ltm);

            start_joint->set_ltm(ltm, i);
        }

        for (auto& child : start_joint->children()) {
            recalculateJoints(child);
        }
    }

}



namespace {

    const std::string kChannels = "CHANNELS";
    const std::string kEnd = "End";
    const std::string kEndSite = "End Site";
    const std::string kFrame = "Frame";
    const std::string kFrames = "Frames:";
    const std::string kHierarchy = "HIERARCHY";
    const std::string kJoint = "JOINT";
    const std::string kMotion = "MOTION";
    const std::string kOffset = "OFFSET";
    const std::string kRoot = "ROOT";

    const std::string kXpos = "Xposition";
    const std::string kYpos = "Yposition";
    const std::string kZpos = "Zposition";
    const std::string kXrot = "Xrotation";
    const std::string kYrot = "Yrotation";
    const std::string kZrot = "Zrotation";

}

#include <iostream>
namespace bvh {


    //##############################################################################
    // Main parse function
    //##############################################################################
    int Bvh_parser::parse(const char* path, Bvh* bvh) {
        // LOG(INFO) << "Parsing file : " << path;

        path_ = path;
        bvh_ = bvh;
        std::string data;
        std::ifstream file(path_);


        if (file.is_open()) {
            std::string token;
            if (file.bad())
                std::cout << "bad file" << std::endl;
            if (file.fail()) {
                std::cout << "fail file" << std::endl;
                std::cout << "Error: " << strerror(errno);
            }
            if (file.eof())
                std::cout << "end file" << std::endl;


#if MULTI_HIERARCHY == 1
            while (file.good()) {
#endif
                file >> token;

                if (token == kHierarchy) {
                    int ret = parseHierarchy(file);
                    if (ret)
                        return ret;
                }
                else {
                    /*LOG(ERROR) << "Bad structure of .bvh file. " << kHierarchy
                        << " should be on the top of the file";*/
                    return -1;
                }
#if MULTI_HIERARCHY == 1
            }
#endif
        }
        else {
            //LOG(ERROR) << "Cannot open file to parse : " << path_;
            return -1;
        }

        //LOG(INFO) << "Successfully parsed file";
        return 0;
    }

    //##############################################################################
    // Function parsing hierarchy
    //##############################################################################
    int Bvh_parser::parseHierarchy(std::ifstream& file) {
        //LOG(INFO) << "Parsing hierarchy";

        std::string token;
        int ret;

        if (file.good()) {
            file >> token;

            //##########################################################################
            // Parsing joints
            //##########################################################################
            if (token == kRoot) {
                std::shared_ptr <Joint> rootJoint;
                ret = parseJoint(file, nullptr, rootJoint);

                if (ret)
                    return ret;

                /* LOG(INFO) << "There is " << bvh_->num_channels() << " data channels in the"
                     << " file";*/

                bvh_->setRootJoint(rootJoint);
            }
            else {
                /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kRoot
                    << ", but found \"" << token << "\"";*/
                return -1;
            }
        }

        if (file.good()) {
            file >> token;

            //##########################################################################
            // Parsing motion data
            //##########################################################################
            if (token == kMotion) {
                ret = parseMotion(file);

                if (ret)
                    return ret;
            }
            else {
                /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kMotion
                    << ", but found \"" << token << "\"";*/
                return -1;
            }
        }
        return 0;
    }

    //##############################################################################
    // Function parsing joint
    //##############################################################################
    int Bvh_parser::parseJoint(std::ifstream& file,
        std::shared_ptr <Joint> parent, std::shared_ptr <Joint>& parsed) {

        /* LOG(TRACE) << "Parsing joint";*/

        std::shared_ptr<Joint> joint = std::make_shared<Joint>();
        joint->set_parent(parent);

        std::string name;
        file >> name;

        /* LOG(TRACE) << "Joint name : " << name;*/

        joint->set_name(name);

        std::string token;
        std::vector <std::shared_ptr <Joint>> children;
        int ret;

        file >> token;  // Consuming '{'
        file >> token;

        //############################################################################
        // Offset parsing
        //############################################################################
        if (token == kOffset) {
            Joint::Offset offset;

            try {
                file >> offset.x >> offset.y >> offset.z;
            }
            catch (const std::ios_base::failure e) {
                /* LOG(ERROR) << "Failure while parsing offset";*/
                return -1;
            }

            joint->set_offset(offset);

            /*LOG(TRACE) << "Offset x: " << offset.x << ", y: " << offset.y << ", z: "
                << offset.z;*/

        }
        else {
            /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kOffset << ", but "
                << "found \"" << token << "\"";*/

            return -1;
        }

        file >> token;

        //############################################################################
        // Channels parsing
        //############################################################################
        if (token == kChannels) {
            ret = parseChannelOrder(file, joint);

            /*LOG(TRACE) << "Joint has " << joint->num_channels() << " data channels";*/

            if (ret)
                return ret;
        }
        else {
            /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kChannels
                << ", but found \"" << token << "\"";*/

            return -1;
        }

        file >> token;

        bvh_->addJoint(joint);

        //############################################################################
        // Children parsing
        //############################################################################

        while (file.good()) {
            //##########################################################################
            // Child joint parsing
            //##########################################################################
            if (token == kJoint) {
                std::shared_ptr <Joint> child;
                ret = parseJoint(file, joint, child);

                if (ret)
                    return ret;

                children.push_back(child);

                //##########################################################################
                // Child joint parsing
                //##########################################################################
            }
            else if (token == kEnd) {
                file >> token >> token;  // Consuming "Site {"

                std::shared_ptr <Joint> tmp_joint = std::make_shared <Joint>();

                tmp_joint->set_parent(joint);
                tmp_joint->set_name(kEndSite);
                children.push_back(tmp_joint);

                file >> token;

                //########################################################################
                // End site offset parsing
                //########################################################################
                if (token == kOffset) {
                    Joint::Offset offset;

                    try {
                        file >> offset.x >> offset.y >> offset.z;
                    }
                    catch (const std::ios_base::failure e) {
                        /*LOG(ERROR) << "Failure while parsing offset";*/
                        return -1;
                    }

                    tmp_joint->set_offset(offset);

                    /*LOG(TRACE) << "Joint name : EndSite";
                    LOG(TRACE) << "Offset x: " << offset.x << ", y: " << offset.y << ", z: "
                        << offset.z;*/

                    file >> token;  // Consuming "}"

                }
                else {
                    /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kOffset
                        << ", but found \"" << token << "\"";*/

                    return -1;
                }

                bvh_->addJoint(tmp_joint);
                //##########################################################################
                // End joint parsing
                //##########################################################################
            }
            else if (token == "}") {
                joint->set_children(children);
                parsed = joint;
                return 0;
            }

            file >> token;
        }

        /*LOG(ERROR) << "Cannot parse joint, unexpected end of file. Last token : "
            << token;*/
        return -1;
    }

    //##############################################################################
    // Motion data parse function
    //##############################################################################
    int Bvh_parser::parseMotion(std::ifstream& file) {

        /*LOG(INFO) << "Parsing motion";*/

        std::string token;
        file >> token;

        int frames_num;

        if (token == kFrames) {
            file >> frames_num;
            bvh_->setNumFrames(frames_num);
            //LOG(INFO) << "Num of frames : " << frames_num;
        }
        else {
            /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kFrames
                << ", but found \"" << token << "\"";*/

            return -1;
        }

        file >> token;

        double frame_time;

        if (token == kFrame) {
            file >> token;  // Consuming 'Time:'
            file >> frame_time;
            bvh_->setFrameTime(frame_time);
            /*LOG(INFO) << "Frame time : " << frame_time;*/

            float number;
            for (int i = 0; i < frames_num; i++) {
                for (auto joint : bvh_->joints()) {
                    std::vector <float> data;
                    for (int j = 0; j < joint->num_channels(); j++) {
                        file >> number;
                        data.push_back(number);
                    }
                    // LOG(TRACE) << joint->name() << ": " << vtos(data);
                    joint->add_frame_motion_data(data);
                }
            }
        }
        else {
            /*LOG(ERROR) << "Bad structure of .bvh file. Expected " << kFrame
                << ", but found \"" << token << "\"";*/

            return -1;
        }

        return 0;
    }

    //##############################################################################
    // Channels order parse function
    //##############################################################################
    int Bvh_parser::parseChannelOrder(std::ifstream& file,
        std::shared_ptr <Joint> joint) {

        /*LOG(TRACE) << "Parse channel order";*/

        int num;
        file >> num;
        /*LOG(TRACE) << "Number of channels : " << num;*/

        std::vector <Joint::Channel> channels;
        std::string token;

        for (int i = 0; i < num; i++) {
            file >> token;
            if (token == kXpos)
                channels.push_back(Joint::Channel::XPOSITION);
            else if (token == kYpos)
                channels.push_back(Joint::Channel::YPOSITION);
            else if (token == kZpos)
                channels.push_back(Joint::Channel::ZPOSITION);
            else if (token == kXrot)
                channels.push_back(Joint::Channel::XROTATION);
            else if (token == kYrot)
                channels.push_back(Joint::Channel::YROTATION);
            else if (token == kZrot)
                channels.push_back(Joint::Channel::ZROTATION);
            else {
                //LOG(ERROR) << "Not valid channel!";
                return -1;
            }
        }

        joint->set_channels_order(channels);
        return 0;
    }

    std::string Bvh_parser::vtos(const std::vector <float>& vector) {
        std::ostringstream oss;

        if (!vector.empty())
        {
            // Convert all but the last element to avoid a trailing ","
            std::copy(vector.begin(), vector.end() - 1,
                std::ostream_iterator<float>(oss, ", "));

            // Now add the last element with no delimiter
            oss << vector.back();
        }

        return oss.str();
    }

} // namespace
 //#include "ply_utils.h"
 //#define TINYPLY_IMPLEMENTATION
 //#include "tinyply.h"
 //#include <fstream>

 //   void write_output_ply(const  std::filesystem::path& file_path, const std::vector<PlyElement>& datas, const std::vector<std::string>& attribute_names)
 //   {
 //       tinyply::PlyFile plyFile;

 //       size_t attribute_offset = 0; // An offset to track the attribute names

 //       for (size_t i = 0; i < datas.size(); ++i) {
 //           // Calculate the number of columns in the tensor.
 //           size_t columns = datas[i].shapes[1];

 //           std::vector<std::string> current_attributes;
 //           for (size_t j = 0; j < columns; ++j) {
 //               current_attributes.push_back(attribute_names[attribute_offset + j]);
 //           }

 //           plyFile.add_properties_to_element(
 //               "vertex",
 //               current_attributes,
 //               tinyply::Type::FLOAT32,
 //               datas[i].shapes[0],
 //               reinterpret_cast<uint8_t*>(datas[i].data),
 //               tinyply::Type::INVALID,
 //               0);

 //           attribute_offset += columns; // Increase the offset for the next tensor.
 //       }

 //       std::filebuf fb;
 //       fb.open(file_path.string(), std::ios::out | std::ios::binary);
 //       std::ostream outputStream(&fb);
 //       plyFile.write(outputStream, true); // 'true' for binary format
 //   }

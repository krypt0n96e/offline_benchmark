Offline ImageNet Benchmark
This project provides a simple tool to benchmark the performance of deep learning models available in torchvision on the ImageNet dataset. The entire evaluation process is conducted offline, requiring the dataset to be prepared locally.

The tool calculates and reports the Top-1 and Top-5 accuracy of the selected model on the ImageNet validation set.

✨ Features
Evaluates Top-1 and Top-5 accuracy.

Supports all model architectures available in torchvision.models (e.g., ResNet, VGG, DenseNet, MobileNet, etc.).

Simple and easy-to-use command-line interface (CLI) powered by the fire library.

Clean, readable, and customizable source code.

🚀 Getting Started
Prerequisites
Python 3.6+

PyTorch

Torchvision

Other libraries listed in requirements.txt.

A downloaded copy of the ImageNet dataset.

Installation
Clone this repository:

git clone https://github.com/krypt0n96e/offline_benchmark.git
cd offline_benchmark

Install the required Python libraries:

pip install -r requirements.txt

Prepare the ImageNet Dataset:

You need to download and extract the full ImageNet dataset (1000 classes).

The directory structure for the validation set (val) must follow the ImageFolder format required by PyTorch. Each subdirectory inside val must correspond to a class.

/path/to/imagenet/
└── val/
    ├── n01440764/
    │   ├── ILSVRC2012_val_00000293.JPEG
    │   └── ...
    ├── n01443537/
    │   └── ...
    └── ...

If your val set contains only image files without subdirectories, you can use this validation script (valprep.sh) to organize it automatically.

🛠️ Usage
You can run the benchmark by executing the imagenet_benchmark.py script with the necessary arguments.

Example: Evaluate the resnet50 model with a batch_size of 256:

python imagenet_benchmark.py --data_root /path/to/imagenet --arch resnet50 --batch_size 256

(Note: Replace /path/to/imagenet with the actual path to your ImageNet data directory.)

A sample run.sh script is also provided for convenience.

Command-Line Arguments
data_root (required): The root path to the ImageNet dataset directory.

arch (default: 'resnet18'): The name of the model architecture to evaluate. The name must match a model in torchvision.models.

batch_size (default: 256): The size of each data batch.

workers (default: 4): The number of sub-processes to use for data loading.

Output
The result will display the progress and the final accuracy on the validation set:

Test: [  0/196]  Time 1.366 (1.366)      Acc@1 75.78 (75.78)     Acc@5 94.92 (94.92)
Test: [ 10/196]  Time 0.789 (0.834)      Acc@1 74.61 (73.49)     Acc@5 94.14 (92.51)
...
Test: [195/196]  Time 0.793 (0.796)      Acc@1 76.56 (76.13)     Acc@5 94.14 (92.86)
 * Acc@1 76.130 Acc@5 92.862

📁 Project Structure
.
├── imagenet_benchmark.py   # Main executable for running the benchmark
├── models.py               # Loads models from torchvision
├── utils.py                # Utility functions (accuracy, average meter)
├── requirements.txt        # List of dependencies
└── run.sh                  # Example script to run the benchmark

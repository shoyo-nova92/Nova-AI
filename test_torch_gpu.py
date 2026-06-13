import torch

print("\n=== TORCH GPU TEST ===\n")

print("Torch:", torch.__version__)
print("CUDA Available:", torch.cuda.is_available())

if torch.cuda.is_available():

    print(
        "Device:",
        torch.cuda.get_device_name(0)
    )

    try:

        x = torch.rand(
            1000,
            1000
        ).cuda()

        y = torch.rand(
            1000,
            1000
        ).cuda()

        z = x @ y

        print(
            "\nGPU COMPUTE SUCCESS"
        )

        print(
            z.shape
        )

    except Exception as e:

        print(
            "\nGPU COMPUTE FAILED"
        )

        print(e)
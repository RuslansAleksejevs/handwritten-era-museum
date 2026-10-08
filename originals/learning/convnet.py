class ConvNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.conv1 = nn.Conv2d(in_channels=3, out_channels=3, kernel_size=(5,5)) #28x28
        self.pool1 = nn.MaxPool2d(kernel_size=(2,2)) # 14x14
        self.conv2 = nn.Conv2d(in_channels=3, out_channels=5, kernel_size=(3,3)) #12x12
        self.pool2 = nn.MaxPool2d(kernel_size=(2,2)) # 6x6
        
        self.flatten = nn.Flatten()

        self.fc1 = nn.Linear(6*6*5, 100)
        self.fc2 = nn.Linear(100, 10)

    
    def forward(self, x):
        # размерность х ~ [64, 3, 32, 32]

        x = F.relu(self.conv1(x))
        x = self.pool1(x)
        x = F.relu(self.conv2(x))
        x = self.pool2(x)

        x = self.flatten(x)

        x = F.relu(self.fc1(x))
        x = self.fc2(x)

        return x
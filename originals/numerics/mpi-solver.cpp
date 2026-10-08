void SendColumn(Matrix &A, Matrix &E, size_t root, size_t n, size_t num) // во все процессы передаем столбец с номером num из процесса root
{
        MPI_Bcast(&E.mat[num][0], n, MPI_DOUBLE, root, MPI_COMM_WORLD);
        MPI_Bcast(&A.mat[num][0], n, MPI_DOUBLE, root, MPI_COMM_WORLD);
}

void Solve(Matrix &A, Matrix &E, size_t rank, size_t size)
{
    size_t n=A.m;
    size_t ind1, ind2;
    vector<double> temp(n); // temp это массив, где будем хранить текущую строку, чтобы находить номер столбца с главным элементом, не собирая всю матрицу заново
    for(size_t i=0; i<n; i++)
        temp[i]=A.mat[i][0];

    for(size_t k=0; k<n; k++)
    {
        if(rank==0) // главный элемент ищет 0-ой процесс
        {
            ind2=k;
            for(size_t s=k+1; s<n; s++)
            {
                if(fabs(temp[s])>fabs(temp[ind2])) ind2=s;
            }
        }
        MPI_Bcast(&ind2, 1, my_MPI_SIZE_T, 0, MPI_COMM_WORLD);
        ind1=k%size;

        SendColumn(A,E,ind1,n,k);
        SendColumn(A,E,ind2%size,n,ind2);

        if( (ind1==rank)||(ind2%size==rank) )
        {
            E.SwapColumns(k,ind2);
            A.SwapColumns(k,ind2);
            E.MultColumn(k,1/A.mat[k][k]);
            A.MultColumn(k,1/A.mat[k][k]);
        }

        for(size_t i=rank; i<n; i=i+size) // заполняем temp для следующей итерации. Каждый процесс делится своими знаниями
        {
            if(k!=n-1)
            {
                temp[i]=A.mat[i][k+1];
            }
        }

        SendColumn(A,E,ind1,n,k); // теперь в следующем цикле все процессы будут знать, как выглядит k-ый столбец, который нужно отнимать
        MPI_Bcast(&temp[k], 1, MPI_DOUBLE, ind1, MPI_COMM_WORLD);
        for(size_t i=rank; i<n; i=i+size) // основное разделение работы между процессами здесь
        {
            if(i!=k)  // if очень важен, иначе появится нулевая строка, но богачеву в книжке пофиг
            {
                E.SubtractColumns(i,k,A.mat[i][k]);
                if(k!=n-1)
                    temp[i]=temp[i]-temp[k]*A.mat[i][k];
                A.SubtractColumns(i,k,A.mat[i][k]);
            }
        }

        for(size_t root=0; root<size; root++) // только массив temp на каждой итерации цикла по k должен быть известен всем процессам
        {
            for(size_t i=root; i<n; i=i+size)
            {
                if(i!=k)
                    MPI_Bcast(&temp[i], 1, MPI_DOUBLE, root, MPI_COMM_WORLD);
            }
        }

    } // A стала единичной, E стала A^(-1)

    for(size_t root=0; root<size; root++) // Каждый процесс сообщает, какими в итоге стали его столбцы
    {
        for(size_t i=root; i<n; i=i+size)
        {
            SendColumn(A,E,root,n,i);
        }
    }

}


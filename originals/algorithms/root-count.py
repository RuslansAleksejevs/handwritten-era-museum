class Solution:
    def rootCount(self, edges: List[List[int]], guesses: List[List[int]], k: int) -> int:
        n=len(edges)+1
        guesses_set=set([tuple(x) for x in guesses])
        edges_set=set()

        g=[[] for i in range(n)] # это не матрица смежности графа. Это матрица соседей и только их. В комменте нижу описываю, что это такое.
        for e in edges:
            g[e[0]].append(e[1])
            g[e[1]].append(e[0])
        # т.е. теперь g это матрица, где g[i] -- список вершин, соединенных с вершиной i

        def dfs1(v,prev):
            for ne in g[v]: # смотрим на всех соседей v
                if(ne==prev): # эта строчка, чтобы не зациклились
                    continue
                p=(v,ne)
                if(p in guesses_set):
                    edges_set.add(p)
                dfs1(ne,v)


        def dfs2(v,prev,k):
            indicator1=False
            indicator2=False
            if( (prev,v) in edges_set ):
                indicator1=True
                edges_set.remove((prev,v))
            if( (v,prev) in guesses_set ):
                indicator2=True
                edges_set.add((v,prev))
            if(len(edges_set)>=k):
                res[0]+=1
            for ne in g[v]:
                if(ne==prev):
                    continue
                dfs2(ne,v,k)
            
            #сначала забыл следующий блок, а он очень важный. В основном коде программы dfs2 мы будем вызывать для всех соседей той вершины, к которой мы применили dfs1, поэтому нужно, чтобы в итоге наше множество было таким, как в самом начале, это очень важно. Вернем его в предыдущее состояние.
            if(indicator1):
                edges_set.add((prev,v))
            if(indicator2):
                edges_set.remove((v,prev))

        res=[0] 
        dfs1(0,-1) #главную работу сделали здесь
        if(len(edges_set)>=k):
            res[0]+=1 

        for ne in g[0]:
            dfs2(ne,0,k)

        return res[0]
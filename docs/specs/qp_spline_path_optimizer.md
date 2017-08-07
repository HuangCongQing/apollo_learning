# QP-Spline-Path Optimizer

Quadratic programming + Spline interpolation

## 1. Objective function 

### 1.1 Get path length 

Path is defined in station-lateral corrdination system. The **s** range from veichle's current position to  default planing path length.

### 1.2  Get spline segments

Split the path into n segments. each segment trajectory is defined by a polynomial.

### 1.3 Define function for each spline segment

Each segment *i* has accumulated distance $d_i$ along reference line. And the trajactory for the segment is defined as a polynomial of degree five by default

$$
l = f_i(s) 
  = a_i + b_i * s + c_i * s^2 + d_i * s^3 + e_i * s^4 + f_i * s^5   (0 \leq s \leq d_{i})
$$

### 1.4 Define objective function of optimization for each segment

$$
cost = \sum_{i=1}^{n} \Big( w_1 \cdot \int\limits_{0}^{d_i} (f_i')^2(s) ds + w_2 \cdot \int\limits_{0}^{d_i} (f_i'')^2(s) ds + w_3 \cdot \int\limits_{0}^{d_i} (f_i^{\prime\prime\prime})^2(s) ds \Big)
$$

### 1.5 Convert the cost function to QP formulation

QP formulation:
$$
\frac{1}{2} \cdot x^T \cdot H \cdot x + f^T \cdot x 
\\
s.t. LB \leq x \leq UB
\\
A_{eq}x = b_{eq}
\\
Ax \leq b
$$
Below is the example for converting cost function to QP formulaiton. 
$$
f_i(s) ＝\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} \cdot  \begin{vmatrix} 1 \\ s \\ s^2 \\ s^3 \\ s^4 \\ s^5 \end{vmatrix}
$$

and
$$
f_i'(s) =\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} \cdot  \begin{vmatrix} 0 \\ 1 \\ s \\ s^2 \\ s^3 \\ s^4 \end{vmatrix}
$$


and 
$$
f_i'(s)^2 =\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} \cdot  \begin{vmatrix} 0 \\ 1 \\ s \\ s^2 \\ s^3 \\ s^4 \end{vmatrix} \cdot \begin{vmatrix} 0 & 1 & s & s^2 & s^3 & s^4 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix}
$$
and 
$$
\int\limits_{0}^{d_i} f_i'(s)^2 ds ＝\int\limits_{0}^{d_i}\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} \cdot  \begin{vmatrix} 0 \\ 1 \\ s \\ s^2 \\ s^3 \\ s^4 \end{vmatrix} \cdot \begin{vmatrix} 0 & 1 & s & s^2 & s^3 & s^4 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix}
$$


and
$$
\int\limits_{0}^{d_i} f'(s)^2 ds ＝\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} \cdot \int\limits_{0}^{d_i}  \begin{vmatrix} 0 \\ 1 \\ s \\ s^2 \\ s^3 \\ s^4 \end{vmatrix} \cdot \begin{vmatrix} 0 & 1 & s & s^2 & s^3 & s^4 \end{vmatrix} ds \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix}
$$


and
$$
\int\limits_{0}^{d_i} 
f'(s)^2 ds ＝\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} 
\cdot \int\limits_{0}^{d_i}
\begin{vmatrix} 
0  & 0 &0&0&0&0\\ 
0 & 1 & s & s^2 & s^3 & s^4\\  
0 & s & s^2 & s^3 & s^4 & s^5\\ 
0 & s^2 &  s^3 & s^4&s^5&s^6 \\ 
0 & s^3 & s^4 &s^5 &s^6&s^7 \\ 
0 & s^4 & s^5 & s^6 & s^7 & s^8 
\end{vmatrix} ds \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix}
$$
and
$$
\int\limits_{0}^{d_i} 
f'_i(s)^2 ds ＝\begin{vmatrix} a_i & b_i & c_i & d_i & e_i & f_i \end{vmatrix} 
\cdot \begin{vmatrix} 
0 & 0 & 0 & 0 &0&0\\ 
0 & d_i & \frac{d_i^2}{2} & \frac{d_i^3}{3} & \frac{d_i^4}{4}&\frac{d_i^5}{5}\\ 
0& \frac{d_i^2}{2} & \frac{d_i^3}{3} & \frac{d_i^4}{4} & \frac{d_i^5}{5}&\frac{d_i^6}{6}\\ 
0& \frac{d_i^3}{3} & \frac{d_i^4}{4} & \frac{d_i^5}{5} & \frac{d_i^6}{6}&\frac{d_i^7}{7}\\ 
0& \frac{d_i^4}{4} & \frac{d_i^5}{5} & \frac{d_i^6}{6} & \frac{d_i^7}{7}&\frac{d_i^8}{8}\\ 
0& \frac{d_i^5}{5} & \frac{d_i^6}{6} & \frac{d_i^7}{7} & \frac{d_i^8}{8}&\frac{d_i^9}{9}\\ 
\end{vmatrix} 
\cdot 
\begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix}
$$

## 2 Constraints  

### 2.1 The init point constraints

Let's assume the the first point is ($s_0$, $l_0$), and $l_0$ should be on the planned path $f_i(s)$, $f'_i(s)$, and $f_i(s)''$.  we convert those constraint into QP equality constraints 
$$
A_{eq}x = b_{eq}
$$
Below is the steps of conversion
$$
f_i(s_0) = \begin{vmatrix} 1 & s_0 & s_0^2 & s_0^3 & s_0^4&s_0^5 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} = l_0
$$
and
$$
f'_i(s_0) = \begin{vmatrix} 0& 1 & s_0 & s_0^2 & s_0^3 & s_0^4 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} = l_0
$$
and 
$$
f''_i(s_0) = \begin{vmatrix} 0&0& 1 & s_0 & s_0^2 & s_0^3  \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} = l_0
$$
$i$ is the index of segment that contains the $s_0$.

so we get the equality constraint as 
$$
\begin{vmatrix} 
 1 & s_0 & s_0^2 & s_0^3 & s_0^4&s_0^5 \\
 0&1 & s_0 & s_0^2 & s_0^3 & s_0^4 \\
 0& 0&1 & s_0 & s_0^2 & s_0^3  
 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} 
 = 
 \begin{vmatrix}
 l_0\\
 l_0\\
 l_0\\
 \end{vmatrix}
$$

### 2.2 The end point constraints

Similar to the init point, the end point $(s_e, l_e)$ is known and  shoud produce the same constraint as above. 

Combined init point and end point, we get equality constraint as 
$$
\begin{vmatrix} 
 1 & s_0 & s_0^2 & s_0^3 & s_0^4&s_0^5 \\
 0&1 & s_0 & s_0^2 & s_0^3 & s_0^4 \\
 0& 0&1 & s_0 & s_0^2 & s_0^3  \\
 1 & s_e & s_e^2 & s_e^3 & s_e^4&s_e^5 \\
 0&1 & s_e & s_e^2 & s_e^3 & s_e^4 \\
 0& 0&1 & s_e & s_e^2 & s_e^3  
 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} 
 = 
 \begin{vmatrix}
 l_0\\
 l_0\\
 l_0\\
 l_e\\
 l_e\\
 l_e\\
 \end{vmatrix}
$$


### 2.3 Joint smoothness  constraints

This constraint is to make the spline joint smooth.  Let's assume two segment $seg_k$ and $seg_{k+1}$ are connected, and the accumulated s of segment $seg_k$ is $s_k$. The we can get the constraint euqation as 
$$
f_k(s_k) = f_{k+1} (s_0)
$$
namely
$$
\begin{vmatrix} 
 1 & s_k & s_k^2 & s_k^3 & s_k^4&s_k^5 \\
 \end{vmatrix} 
 \cdot 
 \begin{vmatrix} 
 a_k \\ b_k \\ c_k \\ d_k \\ e_k \\ f_k 
 \end{vmatrix} 
 = 
\begin{vmatrix} 
 1 & s_{0} & s_{0}^2 & s_{0}^3 & s_{0}^4&s_{0}^5 \\
 \end{vmatrix} 
 \cdot 
 \begin{vmatrix} 
 a_{k+1} \\ b_{k+1} \\ c_{k+1} \\ d_{k+1} \\ e_{k+1} \\ f_{k+1} 
 \end{vmatrix}
$$
then
$$
\begin{vmatrix} 
 1 & s_k & s_k^2 & s_k^3 & s_k^4&s_k^5 &  -1 & -s_{0} & -s_{0}^2 & -s_{0}^3 & -s_{0}^4&-s_{0}^5\\
 \end{vmatrix} 
 \cdot 
 \begin{vmatrix} 
 a_k \\ b_k \\ c_k \\ d_k \\ e_k \\ f_k \\ a_{k+1} \\ b_{k+1} \\ c_{k+1} \\ d_{k+1} \\ e_{k+1} \\ f_{k+1}  
 \end{vmatrix} 
 = 0
$$
$s_0$ = 0 in the equation.

Similarly we can get the equality constraints for 
$$
f'_k(s_k) = f'_{k+1} (s_0)
\\
f''_k(s_k) = f''_{k+1} (s_0)
\\
f'''_k(s_k) = f'''_{k+1} (s_0)
$$

### 2.4 Sampled points for boundary constraint

Evenly sample m points along the path, and check the obstacle boundary at those points.  convert the constraint into QP inequality constraints
$$
Ax \leq b
$$
We first find lower boundary $l_{lb,j}$ at those points ($s_j$, $l_j$) and  $j\in[0, m]$ based on road width and surrounding obstacles. The calculate the inequality constraints as
$$
\begin{vmatrix} 
 1 & s_0 & s_0^2 & s_0^3 & s_0^4&s_0^5 \\
  1 & s_1 & s_1^2 & s_1^3 & s_1^4&s_1^5 \\
 ...&...&...&...&...&... \\
 1 & s_m & s_m^2 & s_m^3 & s_m^4&s_m^5 \\
 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} 
 \leq 
 \begin{vmatrix}
 l_{lb,0}\\
 l_{lb,1}\\
 ...\\
 l_{lb,m}\\
 \end{vmatrix}
$$


similarly, for upper boundary $l_{ub,j}$, we get the ineuqality constraints as 
$$
\begin{vmatrix} 
 1 & s_0 & s_0^2 & s_0^3 & s_0^4&s_0^5 \\
  1 & s_1 & s_1^2 & s_1^3 & s_1^4&s_1^5 \\
 ...&...&...&...&...&... \\
 1 & s_m & s_m^2 & s_m^3 & s_m^4&s_m^5 \\
 \end{vmatrix} \cdot \begin{vmatrix} a_i \\ b_i \\ c_i \\ d_i \\ e_i \\ f_i \end{vmatrix} 
 \leq
 -1 \cdot
 \begin{vmatrix}
 l_{ub,0}\\
 l_{ub,1}\\
 ...\\
 l_{ub,m}\\
 \end{vmatrix}
$$




